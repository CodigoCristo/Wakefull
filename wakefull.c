/*
 * wakefull - Bloqueador de protector de pantalla sencillo
 * 
 * Copyright (C) 2024
 * 
 * Programa simple para prevenir que la pantalla se apague o entre en
 * modo de ahorro de energía. Funciona en X11 y Wayland.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <X11/Xlib.h>

#define PROGRAM_NAME "wakefull"
#define VERSION "2.1.1"
#define PID_FILE "/tmp/wakefull.pid"
#define LOCK_FILE "/tmp/wakefull.lock"
#define MAX_WAIT_SECONDS 10
#define HEALTH_CHECK_INTERVAL 30

typedef enum {
    METHOD_UNKNOWN = 0,
    METHOD_XDG_SCREENSAVER,    // xdg-screensaver (X11)
    METHOD_SYSTEMD_INHIBIT,    // systemd-inhibit (universal)
    METHOD_DBUS_SCREENSAVER,   // D-Bus (universal)
    METHOD_XFCE4_SPECIFIC      // XFCE4 specific methods
} inhibit_method_t;

static pid_t inhibit_pid = 0;
static inhibit_method_t current_method = METHOD_UNKNOWN;
static volatile sig_atomic_t running = 1;
static Display *display = NULL;
static Window x11_window = 0;
static char window_id_str[32] = {0};

// Declaraciones de funciones
int start_wakefull_mode(int debug_mode);
int start_wakefull_debug(void);

// Función para mostrar errores
void print_error(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
}

// Verificar si un comando existe
int command_exists(const char *command) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", command);
    return system(cmd) == 0;
}

// Detectar si estamos en una sesión XFCE4
int is_xfce4_session(void) {
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *session = getenv("DESKTOP_SESSION");
    
    if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce"))) {
        return 1;
    }
    if (session && (strstr(session, "xfce") || strstr(session, "XFCE"))) {
        return 1;
    }
    
    // Verificar si xfconf-query está disponible (indicador de XFCE)
    if (command_exists("xfconf-query")) {
        // Verificar si hay configuraciones activas de XFCE4
        int result = system("xfconf-query -c xfce4-session -l >/dev/null 2>&1");
        return (result == 0);
    }
    
    return 0;
}

// Detectar el mejor método disponible
inhibit_method_t detect_best_method(void) {
    // Para XFCE4, usar método específico (mejor compatibilidad)
    if (is_xfce4_session() && command_exists("xfconf-query")) {
        printf("Detectado entorno XFCE4, usando método específico\n");
        return METHOD_XFCE4_SPECIFIC;
    }
    
    // Preferir systemd-inhibit (funciona en X11 y Wayland)
    if (command_exists("systemd-inhibit")) {
        return METHOD_SYSTEMD_INHIBIT;
    }
    
    // Si hay X11, usar xdg-screensaver
    if (getenv("DISPLAY") && command_exists("xdg-screensaver")) {
        return METHOD_XDG_SCREENSAVER;
    }
    
    // Método D-Bus como respaldo
    if (command_exists("dbus-send")) {
        return METHOD_DBUS_SCREENSAVER;
    }
    
    return METHOD_UNKNOWN;
}

// Crear ventana X11 no mapeada para xdg-screensaver (método de Caffeine)
static int create_x11_window(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        printf("Error: No se puede abrir display X11\n");
        return -1;
    }
    
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    
    // Crear ventana de 1x1 no visible (como hace Caffeine)
    x11_window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0,
                                    BlackPixel(display, screen),
                                    WhitePixel(display, screen));
    
    if (!x11_window) {
        XCloseDisplay(display);
        display = NULL;
        return -1;
    }
    
    // Configurar propiedades de la ventana
    XStoreName(display, x11_window, "wakefull");
    XSetWMProtocols(display, x11_window, NULL, 0);
    
    // Convertir ID a string hexadecimal
    snprintf(window_id_str, sizeof(window_id_str), "0x%lx", x11_window);
    
    return 0;
}

// Limpiar recursos X11
static void cleanup_x11(void) {
    if (x11_window && display) {
        XDestroyWindow(display, x11_window);
        x11_window = 0;
    }
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
    window_id_str[0] = '\0';
}

// Iniciar inhibición con xdg-screensaver (mejorado con técnica de Caffeine)
int start_xdg_inhibition(void) {
    // Crear ventana X11 para xdg-screensaver (como hace Caffeine)
    if (create_x11_window() != 0) {
        printf("Error: No se pudo crear ventana X11\n");
        return -1;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        cleanup_x11();
        return -1;
    } else if (pid == 0) {
        // Proceso hijo - mantener tanto protector como suspensión bloqueados
        while (1) {
            // Suspender protector de pantalla usando ID de ventana apropiado
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend %s >/dev/null 2>&1", window_id_str);
            system(cmd);
            
            // Prevenir suspensión del sistema usando xset (si está disponible)
            system("xset s off >/dev/null 2>&1");
            system("xset -dpms >/dev/null 2>&1");
            system("xset s noblank >/dev/null 2>&1");
            
            // Para XFCE4 específicamente
            if (is_xfce4_session()) {
                system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled -s false >/dev/null 2>&1");
                system("xfconf-query -c xfce4-screensaver -p /saver/enabled -s false >/dev/null 2>&1");
                system("xfconf-query -c xfce4-session -p /shutdown/LockScreen -s false >/dev/null 2>&1");
            }
            
            // También simular actividad de usuario periódicamente
            system("xdotool key shift >/dev/null 2>&1 || true");
            
            sleep(30); // Reactivar cada 30 segundos
        }
    } else {
        // Proceso padre
        inhibit_pid = pid;
        printf("Inhibición iniciada con xdg-screensaver + xset (PID: %d)\n", pid);
        printf("Usando ventana X11: %s\n", window_id_str);
        return 0;
    }
}

// Iniciar inhibición con systemd-inhibit
int start_systemd_inhibition(void) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Proceso hijo - systemd-inhibit mantiene el bloqueo mientras corre
        execlp("systemd-inhibit", "systemd-inhibit", 
               "--what=idle:sleep:handle-lid-switch",
               "--who=wakefull",
               "--why=User requested screen lock prevention",
               "--mode=block",
               "sleep", "infinity", NULL);
        perror("execlp systemd-inhibit");
        exit(1);
    } else {
        // Proceso padre
        inhibit_pid = pid;
        printf("Inhibición iniciada con systemd-inhibit (PID: %d)\n", pid);
        return 0;
    }
}

// Iniciar inhibición con D-Bus
int start_dbus_inhibition(void) {
    // D-Bus para prevenir tanto protector como suspensión
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Proceso hijo - inhibir tanto protector como power management
        while (1) {
            // Inhibir protector de pantalla
            system("dbus-send --session --type=method_call "
                   "--dest=org.freedesktop.ScreenSaver "
                   "/org/freedesktop/ScreenSaver "
                   "org.freedesktop.ScreenSaver.SimulateUserActivity "
                   ">/dev/null 2>&1");
            
            // Inhibir power management (suspensión)
            system("dbus-send --session --type=method_call "
                   "--dest=org.freedesktop.PowerManagement "
                   "/org/freedesktop/PowerManagement/Inhibit "
                   "org.freedesktop.PowerManagement.Inhibit "
                   "string:wakefull string:'Preventing system suspend' "
                   ">/dev/null 2>&1");
            
            // También usar GNOME session manager si está disponible
            system("dbus-send --session --type=method_call "
                   "--dest=org.gnome.SessionManager "
                   "/org/gnome/SessionManager "
                   "org.gnome.SessionManager.Inhibit "
                   "string:wakefull uint32:0 string:'Preventing suspend and screensaver' uint32:12 "
                   ">/dev/null 2>&1");
            
            sleep(30);
        }
    } else {
        // Proceso padre
        inhibit_pid = pid;
        printf("Inhibición iniciada con D-Bus (protector + suspensión) (PID: %d)\n", pid);
        return 0;
    }
}

// Configurar XFCE4 de manera más robusta con manejo de errores
void configure_xfce4_settings(int enable_prevention) {
    if (enable_prevention) {
        printf("Aplicando configuraciones robustas para XFCE4...\n");
        
        // Verificar que xfconf-query esté disponible
        if (!command_exists("xfconf-query")) {
            printf("Error: xfconf-query no disponible, saltando configuración específica de XFCE4\n");
            return;
        }
        
        int errors = 0;
        
        // Configuraciones principales del power manager
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled -s false >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-on-ac-sleep -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-on-battery-sleep -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-ac -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-battery -s 0 >/dev/null 2>&1") != 0) errors++;
        
        // Deshabilitar todas las formas de inactividad
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/inactivity-on-ac -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/inactivity-on-battery -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/inactivity-sleep-mode-on-ac -s 0 >/dev/null 2>&1") != 0) errors++;
        if (system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/inactivity-sleep-mode-on-battery -s 0 >/dev/null 2>&1") != 0) errors++;
        
        // Configuraciones del screensaver (pueden fallar si no está instalado)
        system("xfconf-query -c xfce4-screensaver -p /saver/enabled -s false >/dev/null 2>&1");
        system("xfconf-query -c xfce4-screensaver -p /lock/enabled -s false >/dev/null 2>&1");
        system("xfconf-query -c xfce4-screensaver -p /saver/idle-activation/enabled -s false >/dev/null 2>&1");
        
        // Configuraciones de sesión
        system("xfconf-query -c xfce4-session -p /shutdown/LockScreen -s false >/dev/null 2>&1");
        system("xfconf-query -c xfce4-session -p /general/LockCommand -s '' >/dev/null 2>&1");
        
        // Configuraciones adicionales que pueden causar bloqueo
        system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/lock-screen-suspend-hibernate -s false >/dev/null 2>&1");
        system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/logind-handle-lid-switch -s false >/dev/null 2>&1");
        
        if (errors > 0) {
            printf("Advertencia: %d configuraciones de power manager no pudieron aplicarse\n", errors);
            printf("XFCE4 power manager puede no estar completamente configurado\n");
        } else {
            printf("Configuraciones de XFCE4 aplicadas exitosamente\n");
        }
        
    } else {
        printf("Restaurando configuraciones originales de XFCE4...\n");
        // Las configuraciones se restaurarán en restore_xfce4_settings()
    }
}

// Iniciar inhibición específica para XFCE4
int start_xfce4_inhibition(void) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Proceso hijo - configurar XFCE4 específicamente
        printf("Configurando XFCE4 para prevenir bloqueo...\n");
        
        // Guardar configuraciones actuales para restaurar después
        system("mkdir -p /tmp/wakefull-backup >/dev/null 2>&1");
        system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled > /tmp/wakefull-backup/dpms-enabled 2>/dev/null || echo 'true' > /tmp/wakefull-backup/dpms-enabled");
        system("xfconf-query -c xfce4-screensaver -p /saver/enabled > /tmp/wakefull-backup/saver-enabled 2>/dev/null || echo 'true' > /tmp/wakefull-backup/saver-enabled");
        system("xfconf-query -c xfce4-session -p /shutdown/LockScreen > /tmp/wakefull-backup/lock-screen 2>/dev/null || echo 'true' > /tmp/wakefull-backup/lock-screen");
        
        // Aplicar configuraciones iniciales
        configure_xfce4_settings(1);
        
        while (1) {
            // Reaplicar configuraciones periódicamente (por si se resetean)
            configure_xfce4_settings(1);
            
            // Métodos tradicionales como respaldo
            system("xset s off >/dev/null 2>&1");
            system("xset -dpms >/dev/null 2>&1");
            system("xset s noblank >/dev/null 2>&1");
            system("xset s reset >/dev/null 2>&1");
            
            // Simular actividad de usuario más realista
            system("xdotool key shift >/dev/null 2>&1 || true");
            system("xdotool mousemove_relative 1 1 >/dev/null 2>&1 || true");
            system("xdotool mousemove_relative -- -1 -1 >/dev/null 2>&1 || true");
            
            // D-Bus para XFCE4 y otros servicios
            system("dbus-send --session --type=method_call --dest=org.xfce.PowerManager /org/xfce/PowerManager org.xfce.PowerManager.Inhibit string:'wakefull' string:'Preventing screen lock' >/dev/null 2>&1");
            
            // Inhibir también el screensaver genérico
            system("dbus-send --session --type=method_call --dest=org.freedesktop.ScreenSaver /org/freedesktop/ScreenSaver org.freedesktop.ScreenSaver.SimulateUserActivity >/dev/null 2>&1");
            
            // Prevenir suspensión adicional via logind
            system("dbus-send --system --type=method_call --dest=org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager.Inhibit string:sleep string:idle string:handle-power-key string:handle-suspend-key string:wakefull string:'Preventing system suspend' int32:0 >/dev/null 2>&1 || true");
            
            sleep(30);
        }
    } else {
        // Proceso padre
        inhibit_pid = pid;
        printf("Inhibición específica para XFCE4 iniciada (PID: %d)\n", pid);
        printf("Configuraciones de XFCE4 temporalmente modificadas\n");
        return 0;
    }
}

// Iniciar inhibición según el método detectado
int start_inhibition(void) {
    current_method = detect_best_method();
    
    switch (current_method) {
        case METHOD_XDG_SCREENSAVER:
            return start_xdg_inhibition();
        case METHOD_SYSTEMD_INHIBIT:
            return start_systemd_inhibition();
        case METHOD_DBUS_SCREENSAVER:
            return start_dbus_inhibition();
        case METHOD_XFCE4_SPECIFIC:
            return start_xfce4_inhibition();
        default:
            printf("Error: No se encontró ningún método de inhibición disponible\n");
            return -1;
    }
}

// Detener inhibición
int stop_inhibition(void) {
    if (inhibit_pid > 0) {
        printf("Deteniendo proceso de inhibición (PID: %d)\n", inhibit_pid);
        
        if (kill(inhibit_pid, SIGTERM) == 0) {
            // Esperar a que termine
            int status;
            waitpid(inhibit_pid, &status, 0);
            printf("Inhibición detenida\n");
        } else {
            perror("kill");
            // Intentar cleanup de todas formas
        }
        
        inhibit_pid = 0;
        current_method = METHOD_UNKNOWN;
    }
    
    // Limpieza específica por método
    if (current_method == METHOD_XDG_SCREENSAVER && window_id_str[0] != '\0') {
        // Reactivar screensaver explícitamente (como hace Caffeine)
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s >/dev/null 2>&1", window_id_str);
        system(cmd);
        cleanup_x11();
    }
    
    return 0;
}

// Verificar si wakefull está ejecutándose
// Verificar estado sin hacer limpieza (para diagnóstico)
int check_wakefull_status(int *stored_pid) {
    FILE *file = fopen(PID_FILE, "r");
    if (!file) {
        *stored_pid = 0;
        // Verificar si hay procesos wakefull por nombre sin limpiar
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "pgrep -f 'wakefull --start' | head -1");
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char pid_str[32];
            if (fgets(pid_str, sizeof(pid_str), fp)) {
                *stored_pid = atoi(pid_str);
                pclose(fp);
                return 2; // Proceso existe pero sin PID file
            }
            pclose(fp);
        }
        return 0; // No hay archivo PID ni proceso
    }
    
    if (fscanf(file, "%d", stored_pid) != 1 || *stored_pid <= 0) {
        fclose(file);
        *stored_pid = 0;
        return -1; // Archivo PID corrupto
    }
    fclose(file);
    
    // Verificar si el proceso existe
    if (kill(*stored_pid, 0) == 0) {
        return 1; // PID file existe y proceso activo
    } else {
        return -1; // PID file existe pero proceso muerto
    }
}

int is_wakefull_running(void) {
    FILE *file = fopen(PID_FILE, "r");
    if (!file) {
        // Verificar si hay procesos wakefull por nombre como fallback
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "pgrep -f 'wakefull --start' >/dev/null 2>&1");
        if (system(cmd) == 0) {
            printf("Advertencia: Proceso wakefull detectado sin archivo PID, limpiando...\n");
            system("pkill -f 'wakefull --start' 2>/dev/null");
            sleep(1);
        }
        return 0; // No hay archivo PID, no está ejecutándose
    }
    
    int pid;
    if (fscanf(file, "%d", &pid) != 1 || pid <= 0) {
        fclose(file);
        unlink(PID_FILE); // Archivo corrupto, eliminarlo
        return 0;
    }
    fclose(file);
    
    // Verificar si el proceso existe y es realmente wakefull
    if (kill(pid, 0) == 0) {
        // Verificar que sea realmente nuestro proceso leyendo /proc/pid/comm
        char proc_path[64];
        char process_name[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);
        
        FILE *proc_file = fopen(proc_path, "r");
        if (proc_file) {
            if (fgets(process_name, sizeof(process_name), proc_file)) {
                // Remover newline
                process_name[strcspn(process_name, "\n")] = 0;
                fclose(proc_file);
                
                if (strstr(process_name, PROGRAM_NAME)) {
                    return pid; // Está ejecutándose y es wakefull
                } else {
                    printf("Advertencia: PID %d no es wakefull (es %s), limpiando archivo PID\n", pid, process_name);
                    unlink(PID_FILE);
                    return 0;
                }
            }
            fclose(proc_file);
        }
        
        // Si no podemos leer /proc, asumir que está corriendo
        return pid;
    } else {
        unlink(PID_FILE); // Proceso no existe, eliminar archivo PID
        return 0;
    }
}

// Guardar PID en archivo
int save_pid(void) {
    // Crear lock file para evitar condiciones de carrera
    int lock_fd = open(LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (lock_fd == -1) {
        if (errno == EEXIST) {
            printf("Error: Ya hay una instancia iniciándose, esperando...\n");
            sleep(2);
            // Reintentamos una vez más
            lock_fd = open(LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
            if (lock_fd == -1) {
                printf("Error: No se pudo obtener lock exclusivo\n");
                return -1;
            }
        } else {
            perror("open lock file");
            return -1;
        }
    }
    
    FILE *file = fopen(PID_FILE, "w");
    if (!file) {
        perror("fopen PID file");
        close(lock_fd);
        unlink(LOCK_FILE);
        return -1;
    }
    
    fprintf(file, "%d\n", getpid());
    fflush(file);
    fsync(fileno(file));
    fclose(file);
    
    // Mantener el lock file abierto durante la ejecución
    close(lock_fd);
    return 0;
}

// Diagnosticar problemas del sistema y entorno
void diagnose_system(void) {
    printf("=== Diagnóstico del sistema wakefull ===\n\n");
    
    // Información del entorno
    printf("Entorno detectado:\n");
    printf("  Escritorio: %s\n", getenv("XDG_CURRENT_DESKTOP") ?: "N/A");
    printf("  Sesión: %s\n", getenv("DESKTOP_SESSION") ?: "N/A");
    printf("  Display: %s\n", getenv("DISPLAY") ?: "N/A");
    printf("  Wayland: %s\n", getenv("WAYLAND_DISPLAY") ?: "N/A");
    
    if (is_xfce4_session()) {
        printf("  ✓ XFCE4 detectado - método específico disponible\n\n");
    } else {
        printf("  ⚠ XFCE4 no detectado\n\n");
    }
    
    // Verificar métodos de inhibición disponibles
    printf("Métodos de inhibición:\n");
    
    inhibit_method_t best_method = detect_best_method();
    const char* method_name = "ninguno";
    
    if (command_exists("systemd-inhibit")) {
        printf("  ✓ systemd-inhibit disponible\n");
    } else {
        printf("  ✗ systemd-inhibit no disponible\n");
    }
    
    if (command_exists("xdg-screensaver")) {
        printf("  ✓ xdg-screensaver disponible\n");
    } else {
        printf("  ✗ xdg-screensaver no disponible\n");
    }
    
    if (command_exists("dbus-send")) {
        printf("  ✓ dbus-send disponible\n");
    } else {
        printf("  ✗ dbus-send no disponible\n");
    }
    
    if (is_xfce4_session() && command_exists("xfconf-query")) {
        printf("  ✓ XFCE4-específico disponible\n");
        
        // Verificar acceso a configuraciones XFCE4
        if (system("xfconf-query -c xfce4-power-manager -l >/dev/null 2>&1") == 0) {
            printf("    ✓ xfce4-power-manager configuración accesible\n");
        } else {
            printf("    ✗ xfce4-power-manager configuración no accesible\n");
        }
    } else if (is_xfce4_session()) {
        printf("  ⚠ XFCE4 detectado pero xfconf-query no disponible\n");
        printf("    Instala: sudo apt install xfconf\n");
    }
    
    switch (best_method) {
        case METHOD_SYSTEMD_INHIBIT:
            method_name = "systemd-inhibit";
            break;
        case METHOD_XDG_SCREENSAVER:
            method_name = "xdg-screensaver";
            break;
        case METHOD_DBUS_SCREENSAVER:
            method_name = "D-Bus";
            break;
        case METHOD_XFCE4_SPECIFIC:
            method_name = "XFCE4-específico";
            break;
        default:
            method_name = "ninguno";
            break;
    }
    
    printf("\n  → Método que se usará: %s\n", method_name);
    
    // Estado actual de wakefull
    printf("\nEstado actual:\n");
    
    int stored_pid;
    int status = check_wakefull_status(&stored_pid);
    
    switch (status) {
        case 1:
            printf("  ✓ wakefull está ejecutándose (PID: %d)\n", stored_pid);
            break;
        case 2:
            printf("  ⚠ Proceso wakefull detectado sin PID file (PID: %d)\n", stored_pid);
            printf("    Ejecuta 'wakefull --stop' para limpiar\n");
            break;
        case -1:
            if (stored_pid > 0) {
                printf("  ⚠ PID file existe pero proceso no activo (PID: %d)\n", stored_pid);
            } else {
                printf("  ⚠ PID file corrupto\n");
            }
            printf("    Ejecuta 'wakefull --stop' para limpiar\n");
            break;
        default:
            printf("  ✗ wakefull no está ejecutándose\n");
            break;
    }
    
    // Configuraciones específicas por entorno
    if (is_xfce4_session()) {
        printf("\nConfiguraciones XFCE4 actuales:\n");
        
        char cmd_output[256];
        FILE *fp;
        
        // DPMS
        fp = popen("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled 2>/dev/null", "r");
        if (fp && fgets(cmd_output, sizeof(cmd_output), fp)) {
            if (strstr(cmd_output, "true")) {
                printf("  ⚠ DPMS habilitado (puede causar bloqueo)\n");
            } else {
                printf("  ✓ DPMS deshabilitado\n");
            }
            pclose(fp);
        } else {
            printf("  ? DPMS estado desconocido\n");
        }
        
        // Screensaver
        fp = popen("xfconf-query -c xfce4-screensaver -p /saver/enabled 2>/dev/null", "r");
        if (fp && fgets(cmd_output, sizeof(cmd_output), fp)) {
            if (strstr(cmd_output, "true")) {
                printf("  ⚠ Screensaver habilitado (puede causar bloqueo)\n");
            } else {
                printf("  ✓ Screensaver deshabilitado\n");
            }
            pclose(fp);
        } else {
            printf("  ? Screensaver estado desconocido\n");
        }
    }
    
    printf("\nRecomendaciones por entorno:\n");
    if (is_xfce4_session()) {
        printf("• XFCE4: wakefull usará método específico optimizado\n");
        printf("• Las configuraciones se modifican temporalmente y se restauran automáticamente\n");
    } else if (command_exists("systemd-inhibit")) {
        printf("• Sistema con systemd: método universal recomendado\n");
    } else if (getenv("DISPLAY")) {
        printf("• Sistema X11: usar xdg-screensaver + xset\n");
    } else {
        printf("• Sistema limitado: instalar systemd, xdg-utils o dbus\n");
    }
    
    printf("\nSi hay problemas:\n");
    printf("1. Verificar que el método detectado funciona: wakefull --test\n");
    printf("2. Iniciar wakefull: wakefull --start\n");
    printf("3. Verificar estado: wakefull --status\n");
    if (best_method == METHOD_UNKNOWN) {
        printf("4. ⚠ IMPORTANTE: No hay métodos disponibles - instalar dependencias\n");
    }
}

// Limpiar archivos temporales y recursos
// Restaurar configuraciones originales de XFCE4
void restore_xfce4_settings(void) {
    printf("Restaurando configuraciones originales de XFCE4...\n");
    
    // Restaurar configuraciones principales
    system("if [ -f /tmp/wakefull-backup/dpms-enabled ]; then "
           "xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled -s \"$(cat /tmp/wakefull-backup/dpms-enabled)\" >/dev/null 2>&1; "
           "else "
           "xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-enabled -s true >/dev/null 2>&1; "
           "fi");
    
    system("if [ -f /tmp/wakefull-backup/saver-enabled ]; then "
           "xfconf-query -c xfce4-screensaver -p /saver/enabled -s \"$(cat /tmp/wakefull-backup/saver-enabled)\" >/dev/null 2>&1; "
           "else "
           "xfconf-query -c xfce4-screensaver -p /saver/enabled -s true >/dev/null 2>&1; "
           "fi");
    
    system("if [ -f /tmp/wakefull-backup/lock-screen ]; then "
           "xfconf-query -c xfce4-session -p /shutdown/LockScreen -s \"$(cat /tmp/wakefull-backup/lock-screen)\" >/dev/null 2>&1; "
           "else "
           "xfconf-query -c xfce4-session -p /shutdown/LockScreen -s true >/dev/null 2>&1; "
           "fi");
    
    // Restaurar configuraciones adicionales a valores seguros por defecto
    system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-on-ac-sleep -s 10 >/dev/null 2>&1");
    system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/dpms-on-battery-sleep -s 5 >/dev/null 2>&1");
    system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-ac -s 10 >/dev/null 2>&1");
    system("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/blank-on-battery -s 5 >/dev/null 2>&1");
    
    // Restaurar xset a configuraciones por defecto
    system("xset s on >/dev/null 2>&1");
    system("xset +dpms >/dev/null 2>&1");
    
    // Limpiar directorio de backup
    system("rm -rf /tmp/wakefull-backup >/dev/null 2>&1");
    
    printf("Configuraciones de XFCE4 restauradas\n");
}

void cleanup(void) {
    printf("Realizando limpieza...\n");
    
    // Detener inhibición primero
    if (inhibit_pid > 0) {
        printf("Terminando proceso de inhibición (PID: %d)...\n", inhibit_pid);
        kill(inhibit_pid, SIGTERM);
        
        // Esperar con timeout
        int wait_count = 0;
        while (wait_count < MAX_WAIT_SECONDS && kill(inhibit_pid, 0) == 0) {
            sleep(1);
            wait_count++;
        }
        
        if (kill(inhibit_pid, 0) == 0) {
            printf("Forzando terminación del proceso de inhibición...\n");
            kill(inhibit_pid, SIGKILL);
        }
        inhibit_pid = 0;
    }
    
    // Restaurar configuraciones específicas de XFCE4 si fue el método usado
    if (current_method == METHOD_XFCE4_SPECIFIC) {
        restore_xfce4_settings();
    }
    
    // Limpieza específica por método
    stop_inhibition();
    
    // Limpiar archivos
    unlink(PID_FILE);
    unlink(LOCK_FILE);
    cleanup_x11();
    
    printf("Limpieza completada\n");
}

// Manejador de señales (mejorado con técnica de Caffeine)
void signal_handler(int sig) {
    printf("\nRecibida señal %d (%s), iniciando parada segura...\n", 
           sig, (sig == SIGTERM) ? "TERM" : (sig == SIGINT) ? "INT" : "OTHER");
    
    running = 0; // Señalizar al bucle principal que pare
    
    // Para señales críticas, hacer limpieza inmediata
    if (sig == SIGTERM || sig == SIGINT) {
        cleanup();
        exit(0);
    }
}

// Configurar manejadores de señales
void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    // Ignorar SIGPIPE para evitar crashes
    signal(SIGPIPE, SIG_IGN);
}

// Iniciar wakefull en segundo plano
int start_wakefull(void) {
    return start_wakefull_mode(0);
}

// Iniciar wakefull en modo debug (foreground)
int start_wakefull_debug(void) {
    return start_wakefull_mode(1);
}

// Iniciar wakefull (modo: 0=daemon, 1=foreground)
int start_wakefull_mode(int debug_mode) {
    // Verificar si ya está ejecutándose sin hacer limpieza automática
    int stored_pid;
    int status = check_wakefull_status(&stored_pid);
    if (status == 1) {
        printf("wakefull ya está ejecutándose (PID: %d)\n", stored_pid);
        return 1;
    }
    
    // Limpiar estado previo si es necesario
    if (status == -1 || status == 2) {
        printf("Limpiando estado previo...\n");
        if (status == 2) {
            // Proceso huérfano, terminarlo
            if (stored_pid > 0) {
                kill(stored_pid, SIGTERM);
                sleep(1);
            }
        }
        unlink(PID_FILE);
        unlink(LOCK_FILE);
    }
    
    if (debug_mode) {
        printf("Iniciando wakefull en modo DEBUG (foreground)...\n");
    } else {
        printf("Iniciando wakefull...\n");
    }
    
    // Verificar métodos disponibles antes de continuar
    inhibit_method_t test_method = detect_best_method();
    if (test_method == METHOD_UNKNOWN) {
        printf("Error: No se encontró ningún método de inhibición disponible\n");
        printf("Ejecuta 'wakefull --test' para más información\n");
        return -1;
    }
    
    if (debug_mode) {
        // Modo debug: no hacer fork, ejecutar en foreground
        printf("Modo DEBUG: Ejecutando en primer plano\n");
        printf("Presiona Ctrl+C para detener\n\n");
        
        // Configurar señales
        setup_signals();
        
        // Iniciar inhibición
        if (start_inhibition() != 0) {
            printf("Error: No se pudo iniciar inhibición\n");
            return -1;
        }
        
        const char* method_name = "desconocido";
        inhibit_method_t method = detect_best_method();
        switch (method) {
            case METHOD_SYSTEMD_INHIBIT: method_name = "systemd-inhibit"; break;
            case METHOD_XDG_SCREENSAVER: method_name = "xdg-screensaver"; break;
            case METHOD_DBUS_SCREENSAVER: method_name = "D-Bus"; break;
            case METHOD_XFCE4_SPECIFIC: method_name = "XFCE4-específico"; break;
            default: method_name = "desconocido"; break;
        }
        
        printf("✓ Inhibición iniciada exitosamente\n");
        printf("✓ Método activo: %s\n", method_name);
        printf("✓ PID de inhibición: %d\n", inhibit_pid);
        printf("✓ Protector de pantalla y suspensión bloqueados\n\n");
        
        // Bucle principal con mensajes de debug
        int health_check_counter = 0;
        while (running) {
            sleep(1);
            health_check_counter++;
            
            // Health check cada 30 segundos
            if (health_check_counter >= HEALTH_CHECK_INTERVAL) {
                health_check_counter = 0;
                
                // Verificar si el proceso de inhibición sigue activo
                if (inhibit_pid > 0 && kill(inhibit_pid, 0) != 0) {
                    printf("⚠ Proceso de inhibición terminó inesperadamente, reiniciando...\n");
                    if (start_inhibition() != 0) {
                        printf("✗ Error crítico: No se pudo reiniciar inhibición\n");
                        break;
                    }
                    printf("✓ Inhibición reiniciada\n");
                }
                
                printf("DEBUG: Health check OK - Inhibición activa (PID: %d)\n", inhibit_pid);
            }
        }
        
        printf("\nDeteniendo inhibición...\n");
        cleanup();
        printf("✓ wakefull detenido\n");
        return 0;
    }
    
    // Convertirse en daemon
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid > 0) {
        // Proceso padre - esperar un poco para verificar que el daemon inició correctamente
        sleep(2);
        int new_stored_pid;
        int new_status = check_wakefull_status(&new_stored_pid);
        if (new_status == 1) {
            const char* method_name = "desconocido";
            inhibit_method_t method = detect_best_method();
            switch (method) {
                case METHOD_SYSTEMD_INHIBIT: method_name = "systemd-inhibit"; break;
                case METHOD_XDG_SCREENSAVER: method_name = "xdg-screensaver"; break;
                case METHOD_DBUS_SCREENSAVER: method_name = "D-Bus"; break;
                case METHOD_XFCE4_SPECIFIC: method_name = "XFCE4-específico"; break;
                default: method_name = "desconocido"; break;
            }
            printf("✓ wakefull iniciado correctamente (PID: %d)\n", new_stored_pid);
            printf("✓ Método de inhibición: %s\n", method_name);
            printf("✓ Protector de pantalla y suspensión bloqueados\n");
            printf("\nPara detener: wakefull --stop\n");
            printf("Para estado: wakefull --status\n");
            return 0;
        } else {
            printf("✗ Error: wakefull no pudo iniciarse correctamente\n");
            printf("Ejecuta 'wakefull --diagnose' para más información\n");
            return -1;
        }
    }
    
    // Proceso hijo - continúa como daemon
    setsid(); // Nueva sesión
    
    // Redirigir stdout/stderr para logging
    freopen("/tmp/wakefull.log", "a", stdout);
    freopen("/tmp/wakefull.log", "a", stderr);
    
    printf("\n=== Wakefull daemon iniciado ===\n");
    time_t now = time(NULL);
    printf("Tiempo: %s", ctime(&now));
    
    // Guardar PID
    if (save_pid() != 0) {
        printf("Error: No se pudo guardar PID\n");
        exit(1);
    }
    
    // Configurar señales
    setup_signals();
    
    // Iniciar inhibición
    if (start_inhibition() != 0) {
        printf("Error: No se pudo iniciar inhibición\n");
        cleanup();
        exit(1);
    }
    
    printf("Inhibición iniciada exitosamente con método: %d\n", current_method);
    
    // Bucle principal - mantener vivo el daemon
    int health_check_counter = 0;
    while (running) {
        sleep(1);
        health_check_counter++;
        
        // Health check cada 30 segundos
        if (health_check_counter >= HEALTH_CHECK_INTERVAL) {
            health_check_counter = 0;
            
            // Verificar si el proceso de inhibición sigue activo
            if (inhibit_pid > 0 && kill(inhibit_pid, 0) != 0) {
                printf("Proceso de inhibición terminó inesperadamente (PID: %d), reiniciando...\n", inhibit_pid);
                if (start_inhibition() != 0) {
                    printf("Error crítico: No se pudo reiniciar inhibición\n");
                    break;
                }
            }
            
            printf("Health check OK - Inhibición activa (método: %d, PID: %d)\n", 
                   current_method, inhibit_pid);
        }
    }
    
    printf("Saliendo del bucle principal...\n");
    cleanup();
    return 0;
}

// Detener wakefull
int stop_wakefull(void) {
    int stored_pid;
    int status = check_wakefull_status(&stored_pid);
    
    if (status == 0) {
        printf("✗ wakefull no está ejecutándose\n");
        // Limpiar archivos residuales por si acaso
        unlink(PID_FILE);
        unlink(LOCK_FILE);
        return 1;
    }
    
    if (status == -1) {
        printf("Limpiando estado corrupto...\n");
        unlink(PID_FILE);
        unlink(LOCK_FILE);
        if (stored_pid > 0 && kill(stored_pid, 0) == 0) {
            kill(stored_pid, SIGTERM);
        }
        printf("✓ Estado limpiado\n");
        return 0;
    }
    
    if (status == 2) {
        // Proceso huérfano
        printf("Deteniendo proceso huérfano (PID: %d)...\n", stored_pid);
        if (kill(stored_pid, SIGTERM) == 0) {
            sleep(2);
            printf("✓ Proceso terminado\n");
        }
        unlink(PID_FILE);
        unlink(LOCK_FILE);
        return 0;
    }
    
    // status == 1: proceso normal ejecutándose
    printf("Deteniendo wakefull (PID: %d)...\n", stored_pid);
    
    if (kill(stored_pid, SIGTERM) == 0) {
        // Esperar silenciosamente con timeout
        int wait_count = 0;
        while (wait_count < 5) {
            sleep(1);
            int new_stored_pid;
            if (check_wakefull_status(&new_stored_pid) == 0) {
                printf("✓ wakefull detenido correctamente\n");
                return 0;
            }
            wait_count++;
        }
        
        // Si no respondió, forzar detención
        printf("Forzando detención...\n");
        if (kill(stored_pid, SIGKILL) == 0) {
            sleep(1);
            printf("✓ wakefull detenido\n");
            unlink(PID_FILE);
            unlink(LOCK_FILE);
            return 0;
        }
    }
    
    printf("✗ Error: No se pudo detener wakefull\n");
    return -1;
}

// Mostrar estado
void print_status(void) {
    int stored_pid;
    int status = check_wakefull_status(&stored_pid);
    
    switch (status) {
        case 1:
            printf("✓ Estado: wakefull está EJECUTÁNDOSE (PID: %d)\n", stored_pid);
            
            // Mostrar método usado si es posible determinar
            const char* method_name = "desconocido";
            inhibit_method_t method = detect_best_method();
            switch (method) {
                case METHOD_XDG_SCREENSAVER:
                    method_name = "xdg-screensaver";
                    break;
                case METHOD_SYSTEMD_INHIBIT:
                    method_name = "systemd-inhibit";
                    break;
                case METHOD_DBUS_SCREENSAVER:
                    method_name = "D-Bus";
                    break;
                case METHOD_XFCE4_SPECIFIC:
                    method_name = "XFCE4-específico";
                    break;
                default:
                    break;
            }
            printf("✓ Método activo: %s\n", method_name);
            printf("✓ Protector de pantalla y suspensión bloqueados\n");
            break;
        case 2:
            printf("⚠ Estado: Proceso wakefull detectado sin PID file (PID: %d)\n", stored_pid);
            printf("  Ejecuta 'wakefull --stop' para limpiar\n");
            break;
        case -1:
            if (stored_pid > 0) {
                printf("⚠ Estado: PID file existe pero proceso no activo (PID: %d)\n", stored_pid);
            } else {
                printf("⚠ Estado: PID file corrupto\n");
            }
            printf("  Ejecuta 'wakefull --stop' para limpiar\n");
            break;
        default:
            printf("✗ Estado: wakefull NO está ejecutándose\n");
            break;
    }
}

// Probar métodos disponibles
void test_methods(void) {
    printf("Probando métodos de inhibición disponibles:\n\n");
    
    printf("Entorno detectado:\n");
    if (getenv("DISPLAY")) {
        printf("  ✓ X11 (DISPLAY=%s)\n", getenv("DISPLAY"));
    } else {
        printf("  - X11 no detectado\n");
    }
    
    if (getenv("WAYLAND_DISPLAY")) {
        printf("  ✓ Wayland (WAYLAND_DISPLAY=%s)\n", getenv("WAYLAND_DISPLAY"));
    } else {
        printf("  - Wayland no detectado\n");
    }
    
    if (is_xfce4_session()) {
        printf("  ✓ XFCE4 detectado\n");
        const char *desktop = getenv("XDG_CURRENT_DESKTOP");
        const char *session = getenv("DESKTOP_SESSION");
        if (desktop) printf("    - XDG_CURRENT_DESKTOP=%s\n", desktop);
        if (session) printf("    - DESKTOP_SESSION=%s\n", session);
    } else {
        printf("  - XFCE4 no detectado\n");
    }
    
    printf("\nMétodos disponibles:\n");
    
    if (is_xfce4_session() && command_exists("xfconf-query")) {
        printf("  ✓ XFCE4-específico (configuración nativa XFCE4 - recomendado para XFCE)\n");
        printf("    - Modifica temporalmente configuraciones de xfce4-power-manager\n");
        printf("    - Controla xfce4-screensaver directamente\n");
        printf("    - Restaura configuraciones automáticamente al parar\n");
    } else if (is_xfce4_session()) {
        printf("  ✗ XFCE4-específico disponible pero falta xfconf-query\n");
    }
    
    if (command_exists("systemd-inhibit")) {
        printf("  ✓ systemd-inhibit (protector + suspensión + tapa - recomendado universal)\n");
    } else {
        printf("  ✗ systemd-inhibit no disponible\n");
    }
    
    if (command_exists("xdg-screensaver")) {
        printf("  ✓ xdg-screensaver + xset (protector + suspensión con X11)\n");
        if (getenv("DISPLAY")) {
            printf("    - X11 detectado: método completamente funcional\n");
        } else {
            printf("    - Advertencia: X11 no detectado, puede fallar\n");
        }
    } else {
        printf("  ✗ xdg-screensaver no disponible\n");
    }
    
    if (command_exists("dbus-send")) {
        printf("  ✓ D-Bus (protector + suspensión - alternativo)\n");
    } else {
        printf("  ✗ D-Bus no disponible\n");
    }
    
    inhibit_method_t best_method = detect_best_method();
    if (best_method != METHOD_UNKNOWN) {
        const char* method_name = "desconocido";
        switch (best_method) {
            case METHOD_XDG_SCREENSAVER:
                method_name = "xdg-screensaver";
                break;
            case METHOD_SYSTEMD_INHIBIT:
                method_name = "systemd-inhibit";
                break;
            case METHOD_DBUS_SCREENSAVER:
                method_name = "D-Bus";
                break;
            case METHOD_XFCE4_SPECIFIC:
                method_name = "XFCE4-específico";
                break;
            default:
                break;
        }
        printf("\nMétodo que se usará: %s\n", method_name);
    } else {
        printf("\n⚠ ADVERTENCIA: No hay métodos disponibles\n");
        printf("Instala al menos uno de estos paquetes:\n");
        printf("  - systemd (para systemd-inhibit)\n");
        printf("  - xdg-utils (para xdg-screensaver)\n");
        printf("  - dbus (para D-Bus)\n");
    }
}

// Mostrar ayuda
void print_usage(void) {
    printf("Uso: %s [OPCIÓN]\n", PROGRAM_NAME);
    printf("\n");
    printf("Bloqueador simple de protector de pantalla\n");
    printf("\n");
    printf("\nOpciones:\n");
    printf("  --start     Iniciar bloqueo (protector + suspensión)\n");
    printf("  --stop      Detener bloqueo\n");
    printf("  --status    Ver estado actual\n");
    printf("  --test      Probar métodos disponibles\n");
    printf("  --diagnose  Diagnosticar problemas específicos del entorno\n");
    printf("  --debug     Ejecutar en modo foreground (para debugging)\n");
    printf("  --help      Mostrar esta ayuda\n");
    printf("  --version   Ver versión\n");
    printf("\n");
    printf("Ejemplos:\n");
    printf("  wakefull --start    # Iniciar en segundo plano\n", PROGRAM_NAME);
    printf("  wakefull --status   # Ver si está ejecutándose\n", PROGRAM_NAME);
    printf("  wakefull --stop     # Detener\n", PROGRAM_NAME);
    printf("  wakefull --debug    # Ejecutar en primer plano\n", PROGRAM_NAME);
    printf("  wakefull --diagnose # Diagnosticar problemas\n", PROGRAM_NAME);
    printf("\n");
    printf("El programa previene:\n");
    printf("  • Protector de pantalla (screensaver)\n");
    printf("  • Suspensión del sistema (sleep/hibernación)\n"); 
    printf("  • Suspensión por cierre de tapa (en laptops)\n");
    printf("Detecta automáticamente el mejor método para tu entorno.\n");
    printf("\n");
    if (is_xfce4_session()) {
        printf("XFCE4 detectado - Método específico disponible:\n");
        printf("  • Configuración nativa de xfce4-power-manager\n");
        printf("  • Control directo de xfce4-screensaver\n");
        printf("  • Restauración automática al parar\n");
        printf("  • Usar --diagnose si hay problemas persistentes\n");
    }
}

// Mostrar versión
void print_version(void) {
    printf("%s %s\n", PROGRAM_NAME, VERSION);
    printf("Bloqueador de protector de pantalla y suspensión del sistema\n");
}

// Función principal
int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    } else if (strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    } else if (strcmp(argv[1], "--status") == 0) {
        print_status();
        return 0;
    } else if (strcmp(argv[1], "--test") == 0) {
        test_methods();
        return 0;
    } else if (strcmp(argv[1], "--diagnose") == 0) {
        diagnose_system();
        return 0;
    } else if (strcmp(argv[1], "--debug") == 0) {
        return start_wakefull_debug();
    } else if (strcmp(argv[1], "--start") == 0) {
        return start_wakefull();
    } else if (strcmp(argv[1], "--stop") == 0) {
        return stop_wakefull();
    } else {
        fprintf(stderr, "%s: Opción desconocida '%s'\n", PROGRAM_NAME, argv[1]);
        print_usage();
        return 1;
    }
    
    return 0;
}