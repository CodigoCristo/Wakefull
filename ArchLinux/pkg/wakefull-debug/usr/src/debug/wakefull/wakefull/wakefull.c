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
#include <X11/Xlib.h>

#define PROGRAM_NAME "wakefull"
#define VERSION "2.1.0"
#define PID_FILE "/tmp/wakefull.pid"

typedef enum {
    METHOD_UNKNOWN = 0,
    METHOD_XDG_SCREENSAVER,    // xdg-screensaver (X11)
    METHOD_SYSTEMD_INHIBIT,    // systemd-inhibit (universal)
    METHOD_DBUS_SCREENSAVER    // D-Bus (universal)
} inhibit_method_t;

static pid_t inhibit_pid = 0;
static inhibit_method_t current_method = METHOD_UNKNOWN;
static Display *display = NULL;
static Window x11_window = 0;
static char window_id_str[32] = {0};

// Verificar si un comando existe
int command_exists(const char *command) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s > /dev/null 2>&1", command);
    return system(cmd) == 0;
}

// Detectar el mejor método disponible
inhibit_method_t detect_best_method(void) {
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
int is_wakefull_running(void) {
    FILE *file = fopen(PID_FILE, "r");
    if (!file) {
        return 0; // No hay archivo PID, no está ejecutándose
    }
    
    int pid;
    if (fscanf(file, "%d", &pid) != 1) {
        fclose(file);
        unlink(PID_FILE); // Archivo corrupto, eliminarlo
        return 0;
    }
    fclose(file);
    
    // Verificar si el proceso existe
    if (kill(pid, 0) == 0) {
        return pid; // Está ejecutándose
    } else {
        unlink(PID_FILE); // Proceso no existe, eliminar archivo PID
        return 0;
    }
}

// Guardar PID en archivo
int save_pid(void) {
    FILE *file = fopen(PID_FILE, "w");
    if (!file) {
        perror("fopen");
        return -1;
    }
    
    fprintf(file, "%d\n", getpid());
    fclose(file);
    return 0;
}

// Limpiar archivos temporales y recursos
void cleanup(void) {
    unlink(PID_FILE);
    cleanup_x11();
}

// Manejador de señales (mejorado con técnica de Caffeine)
void signal_handler(int sig) {
    printf("\nRecibida señal %d, limpiando y saliendo...\n", sig);
    
    // Detener inhibición primero
    if (inhibit_pid > 0) {
        kill(inhibit_pid, SIGTERM);
        waitpid(inhibit_pid, NULL, 0);
    }
    
    // Limpieza completa
    cleanup();
    exit(0);
}

// Configurar manejadores de señales
void setup_signals(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
}

// Iniciar wakefull en segundo plano
int start_wakefull(void) {
    int running_pid = is_wakefull_running();
    if (running_pid > 0) {
        printf("wakefull ya está ejecutándose (PID: %d)\n", running_pid);
        return 1;
    }
    
    printf("Iniciando wakefull...\n");
    
    // Convertirse en daemon
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid > 0) {
        // Proceso padre - mostrar info y salir
        printf("wakefull iniciado en segundo plano (PID: %d)\n", pid);
        return 0;
    }
    
    // Proceso hijo - continúa como daemon
    setsid(); // Nueva sesión
    
    // Guardar PID
    if (save_pid() != 0) {
        exit(1);
    }
    
    // Configurar señales
    setup_signals();
    
    // Iniciar inhibición
    if (start_inhibition() != 0) {
        cleanup();
        exit(1);
    }
    
    // Bucle principal - mantener vivo el daemon
    while (1) {
        sleep(60); // Dormir 1 minuto
        
        // Verificar si el proceso de inhibición sigue activo
        if (inhibit_pid > 0 && kill(inhibit_pid, 0) != 0) {
            printf("Proceso de inhibición terminó inesperadamente, reiniciando...\n");
            start_inhibition();
        }
    }
    
    return 0;
}

// Detener wakefull
int stop_wakefull(void) {
    int running_pid = is_wakefull_running();
    if (running_pid == 0) {
        printf("wakefull no está ejecutándose\n");
        return 1;
    }
    
    printf("Deteniendo wakefull (PID: %d)...\n", running_pid);
    
    if (kill(running_pid, SIGTERM) == 0) {
        // Esperar un momento para que termine
        sleep(1);
        
        // Verificar si realmente terminó
        if (is_wakefull_running() == 0) {
            printf("wakefull detenido\n");
            return 0;
        } else {
            printf("Forzando detención...\n");
            kill(running_pid, SIGKILL);
            sleep(1);
            cleanup();
            return 0;
        }
    } else {
        perror("kill");
        return -1;
    }
}

// Mostrar estado
void print_status(void) {
    int running_pid = is_wakefull_running();
    
    if (running_pid > 0) {
        printf("Estado: wakefull está EJECUTÁNDOSE (PID: %d)\n", running_pid);
        
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
            default:
                break;
        }
        printf("Método probable: %s\n", method_name);
    } else {
        printf("Estado: wakefull NO está ejecutándose\n");
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
    
    printf("\nMétodos disponibles:\n");
    
    if (command_exists("systemd-inhibit")) {
        printf("  ✓ systemd-inhibit (protector + suspensión + tapa - recomendado)\n");
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
    printf("  --help      Mostrar esta ayuda\n");
    printf("  --version   Ver versión\n");
    printf("\n");
    printf("Ejemplos:\n");
    printf("  %s --start    # Iniciar en segundo plano\n", PROGRAM_NAME);
    printf("  %s --status   # Ver si está ejecutándose\n", PROGRAM_NAME);
    printf("  %s --stop     # Detener\n", PROGRAM_NAME);
    printf("\n");
    printf("El programa previene:\n");
    printf("  • Protector de pantalla (screensaver)\n");
    printf("  • Suspensión del sistema (sleep/hibernación)\n"); 
    printf("  • Suspensión por cierre de tapa (en laptops)\n");
    printf("Detecta automáticamente el mejor método para tu entorno.\n");
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