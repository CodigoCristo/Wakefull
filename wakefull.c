/*
 * wakefull - Bloqueador de protector de pantalla simple para XFCE4
 * 
 * Copyright (C) 2024
 * 
 * Programa simple para prevenir que la pantalla se apague o entre en
 * modo de ahorro de energía, específicamente optimizado para XFCE4.
 * Funciona como caffeine.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#define PROGRAM_NAME "wakefull"
#define VERSION "2.2.0"
#define PID_FILE "/tmp/wakefull.pid"
#define LOG_FILE "/tmp/wakefull.log"
#define ACTIVITY_INTERVAL 30  // segundos entre actividades

static volatile sig_atomic_t running = 1;
static int debug_mode = 0;

// Función para logging
void log_message(const char *level, const char *message) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';  // Remover \n
    
    if (debug_mode) {
        printf("[%s] %s: %s\n", time_str, level, message);
    } else {
        FILE *log = fopen(LOG_FILE, "a");
        if (log) {
            fprintf(log, "[%s] %s: %s\n", time_str, level, message);
            fclose(log);
        }
    }
}

// Manejador de señales
void signal_handler(int sig) {
    (void)sig; // Evitar warning de parámetro no usado
    log_message("INFO", "Señal recibida, terminando...");
    running = 0;
}

// Configurar manejadores de señales
void setup_signals(void) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
}

// Guardar PID del daemon
int save_pid(void) {
    FILE *f = fopen(PID_FILE, "w");
    if (!f) {
        log_message("ERROR", "No se pudo crear archivo PID");
        return -1;
    }
    fprintf(f, "%d\n", getpid());
    fclose(f);
    return 0;
}

// Leer PID del archivo
int read_pid(void) {
    FILE *f = fopen(PID_FILE, "r");
    if (!f) return 0;
    
    int pid;
    if (fscanf(f, "%d", &pid) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return pid;
}

// Verificar si wakefull está ejecutándose
int is_running(void) {
    int pid = read_pid();
    if (pid <= 0) return 0;
    
    // Verificar si el proceso existe
    if (kill(pid, 0) == 0) {
        return pid;
    }
    
    // PID no válido, limpiar archivo
    unlink(PID_FILE);
    return 0;
}

// Ejecutar comando del sistema de forma segura
int safe_system(const char *command) {
    int result = system(command);
    if (result == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error ejecutando: %s", command);
        log_message("WARNING", msg);
        return -1;
    }
    return WEXITSTATUS(result);
}

// Mantener el sistema activo usando métodos múltiples
void keep_system_awake(void) {
    static int method_counter = 0;
    method_counter++;
    
    // Método 1: xset (resetear screensaver y activar DPMS)
    if (safe_system("command -v xset >/dev/null 2>&1") == 0) {
        safe_system("xset s reset 2>/dev/null");
        safe_system("xset dpms force on 2>/dev/null");
        if (debug_mode && method_counter % 10 == 0) {
            log_message("DEBUG", "xset reset ejecutado");
        }
    }
    
    // Método 2: Desactivar temporalmente screensaver de XFCE4
    if (safe_system("command -v xfconf-query >/dev/null 2>&1") == 0) {
        // Desactivar blank screen temporalmente
        safe_system("xfconf-query -c xfce4-screensaver -p /saver/enabled -s false 2>/dev/null");
        safe_system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-ac -s 0 2>/dev/null");
        safe_system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-battery -s 0 2>/dev/null");
    }
    
    // Método 3: xdg-screensaver como respaldo
    if (safe_system("command -v xdg-screensaver >/dev/null 2>&1") == 0) {
        safe_system("xdg-screensaver reset 2>/dev/null");
    }
    
    // Método 4: Crear actividad del sistema (tocar archivo)
    safe_system("touch /tmp/.wakefull_keepalive 2>/dev/null");
    
    // Log cada 10 ciclos en modo debug
    if (debug_mode && method_counter % 10 == 0) {
        log_message("DEBUG", "Sistema mantenido activo");
    }
}

// Restaurar configuración original de XFCE4
void restore_xfce4_settings(void) {
    log_message("INFO", "Restaurando configuración original de XFCE4...");
    
    if (safe_system("command -v xfconf-query >/dev/null 2>&1") == 0) {
        // Restaurar configuraciones por defecto
        safe_system("xfconf-query -c xfce4-screensaver -p /saver/enabled -s true 2>/dev/null");
        safe_system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-ac -s 10 2>/dev/null");
        safe_system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-battery -s 5 2>/dev/null");
    }
    
    // Limpiar archivos temporales
    unlink("/tmp/.wakefull_keepalive");
    unlink(PID_FILE);
}

// Limpiar al salir
void cleanup(void) {
    log_message("INFO", "Limpiando recursos...");
    restore_xfce4_settings();
}

// Bucle principal del daemon
void daemon_loop(void) {
    log_message("INFO", "Iniciando bucle principal del daemon");
    
    int cycle_count = 0;
    while (running) {
        keep_system_awake();
        
        cycle_count++;
        if (cycle_count % 100 == 0) {  // Log cada ~50 minutos
            char msg[64];
            snprintf(msg, sizeof(msg), "Daemon activo - ciclo %d", cycle_count);
            log_message("INFO", msg);
        }
        
        // Dormir en intervalos pequeños para respuesta rápida a señales
        for (int i = 0; i < ACTIVITY_INTERVAL && running; i++) {
            sleep(1);
        }
    }
    
    log_message("INFO", "Bucle principal terminado");
}

// Iniciar como daemon
int start_daemon(void) {
    int existing_pid = is_running();
    if (existing_pid) {
        printf("wakefull ya está ejecutándose (PID: %d)\n", existing_pid);
        return 1;
    }
    
    printf("Iniciando wakefull para XFCE4...\n");
    
    // Verificar que estamos en X11
    if (!getenv("DISPLAY")) {
        printf("Error: No se detectó sesión X11\n");
        return -1;
    }
    
    // Verificar comandos necesarios
    if (safe_system("command -v xset >/dev/null 2>&1") != 0) {
        printf("Advertencia: xset no encontrado, funcionalidad limitada\n");
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al crear daemon");
        return -1;
    }
    
    if (pid > 0) {
        // Proceso padre - esperar confirmación
        sleep(2);
        int new_pid = is_running();
        if (new_pid) {
            printf("✓ wakefull iniciado exitosamente (PID: %d)\n", new_pid);
            printf("✓ Bloqueo de pantalla y suspensión desactivados\n");
            printf("✓ Log: %s\n", LOG_FILE);
            printf("\nPara detener: wakefull --stop\n");
            return 0;
        } else {
            printf("Error: No se pudo iniciar wakefull\n");
            return -1;
        }
    }
    
    // Proceso hijo - daemon
    setsid();
    chdir("/tmp");
    
    // Redirigir salida estándar
    if (!debug_mode) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDONLY);  // stdin
        open("/dev/null", O_WRONLY);  // stdout
        open("/dev/null", O_WRONLY);  // stderr
    }
    
    // Guardar PID
    if (save_pid() < 0) {
        exit(1);
    }
    
    // Configurar señales
    setup_signals();
    
    log_message("INFO", "Daemon wakefull iniciado");
    
    // Bucle principal
    daemon_loop();
    
    // Cleanup al salir
    cleanup();
    log_message("INFO", "Daemon wakefull terminado");
    
    exit(0);
}

// Detener daemon
int stop_daemon(void) {
    int pid = is_running();
    if (!pid) {
        printf("wakefull no está ejecutándose\n");
        return 1;
    }
    
    printf("Deteniendo wakefull (PID: %d)...\n", pid);
    
    if (kill(pid, SIGTERM) < 0) {
        perror("Error al enviar señal");
        return -1;
    }
    
    // Esperar terminación
    for (int i = 0; i < 5; i++) {
        sleep(1);
        if (!is_running()) {
            printf("✓ wakefull detenido exitosamente\n");
            return 0;
        }
    }
    
    // Forzar si no responde
    printf("Forzando terminación...\n");
    kill(pid, SIGKILL);
    sleep(1);
    
    // Limpiar archivos
    unlink(PID_FILE);
    printf("✓ wakefull terminado forzosamente\n");
    
    return 0;
}

// Mostrar estado
void show_status(void) {
    int pid = is_running();
    if (pid) {
        printf("Estado: ACTIVO\n");
        printf("PID: %d\n", pid);
        printf("Log: %s\n", LOG_FILE);
        
        // Mostrar info adicional si el log existe
        struct stat st;
        if (stat(LOG_FILE, &st) == 0) {
            printf("Tamaño del log: %ld bytes\n", st.st_size);
            printf("Última modificación: %s", ctime(&st.st_mtime));
        }
    } else {
        printf("Estado: INACTIVO\n");
    }
}

// Ejecutar en primer plano (debug)
int run_foreground(void) {
    if (is_running()) {
        printf("Error: wakefull ya está ejecutándose como daemon\n");
        printf("Usa 'wakefull --stop' primero\n");
        return 1;
    }
    
    printf("Ejecutando wakefull en primer plano (Ctrl+C para detener)\n");
    printf("Optimizado para XFCE4\n\n");
    
    debug_mode = 1;
    setup_signals();
    
    log_message("INFO", "Wakefull iniciado en modo debug");
    daemon_loop();
    cleanup();
    
    return 0;
}

// Mostrar ayuda
void show_help(void) {
    printf("wakefull %s - Bloqueador de protector de pantalla para XFCE4\n\n", VERSION);
    printf("Uso: %s [OPCIÓN]\n\n", PROGRAM_NAME);
    printf("Opciones:\n");
    printf("  --start, -s      Iniciar daemon\n");
    printf("  --stop, -t       Detener daemon\n");
    printf("  --status         Mostrar estado\n");
    printf("  --foreground, -f Ejecutar en primer plano (debug)\n");
    printf("  --help, -h       Mostrar esta ayuda\n");
    printf("  --version, -v    Mostrar versión\n");
    printf("\nEjemplos:\n");
    printf("  %s --start       # Iniciar daemon\n", PROGRAM_NAME);
    printf("  %s --stop        # Detener daemon\n", PROGRAM_NAME);
    printf("  %s --status      # Ver estado actual\n", PROGRAM_NAME);
    printf("\nEste programa previene que XFCE4 active el protector de pantalla\n");
    printf("o ponga la pantalla en modo de ahorro de energía.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    if (strcmp(argv[1], "--start") == 0 || strcmp(argv[1], "-s") == 0) {
        return start_daemon();
    } else if (strcmp(argv[1], "--stop") == 0 || strcmp(argv[1], "-t") == 0) {
        return stop_daemon();
    } else if (strcmp(argv[1], "--status") == 0) {
        show_status();
        return 0;
    } else if (strcmp(argv[1], "--foreground") == 0 || strcmp(argv[1], "-f") == 0) {
        return run_foreground();
    } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        show_help();
        return 0;
    } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("%s versión %s\n", PROGRAM_NAME, VERSION);
        return 0;
    } else {
        printf("Opción desconocida: %s\n", argv[1]);
        printf("Usa '%s --help' para ver las opciones disponibles.\n", PROGRAM_NAME);
        return 1;
    }
}