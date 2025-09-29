/*
 * Wakefull - Mantén tu computadora despierta
 * 
 * Basado en la funcionalidad de Caffeine
 * 
 * Uso:
 *   wakefull --start    Iniciar prevención de inactividad del escritorio
 *   wakefull --stop     Detener prevención de inactividad del escritorio
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <sys/types.h>

#define PROGRAM_NAME "wakefull"
#define LOCK_FILE "/tmp/wakefull.lock"
#define MAX_CMD_LEN 256
#define MAX_ID_LEN 32
#define MAX_PID_LEN 16

void print_usage() {
    printf("Uso: %s [OPCIÓN]\n", PROGRAM_NAME);
    printf("Mantén tu computadora despierta previniendo la inactividad del escritorio.\n\n");
    printf("Opciones:\n");
    printf("  --start    Iniciar prevención de inactividad del escritorio (modo daemon)\n");
    printf("  --stop     Detener prevención de inactividad del escritorio\n");
    printf("  --status   Mostrar estado actual\n");
    printf("  --help     Mostrar esta ayuda y salir\n");
    printf("\n");
}

int is_running() {
    return access(LOCK_FILE, F_OK) == 0;
}

char* get_window_id() {
    static char window_id[MAX_ID_LEN];
    FILE *file = fopen(LOCK_FILE, "r");
    if (file == NULL) {
        return NULL;
    }
    
    // Skip PID line, read window ID
    char line[MAX_PID_LEN];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return NULL;
    }
    
    if (fgets(window_id, sizeof(window_id), file) == NULL) {
        fclose(file);
        return NULL;
    }
    
    fclose(file);
    
    // Remove newline if present
    int len = strlen(window_id);
    if (len > 0 && window_id[len-1] == '\n') {
        window_id[len-1] = '\0';
    }
    
    return window_id;
}

void save_window_id(const char* window_id) {
    FILE *file = fopen(LOCK_FILE, "w");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot create lock file\n");
        exit(1);
    }
    
    fprintf(file, "%d\n%s\n", getpid(), window_id);
    fclose(file);
}

pid_t get_daemon_pid() {
    FILE *file = fopen(LOCK_FILE, "r");
    if (file == NULL) {
        return -1;
    }
    
    char pid_str[MAX_PID_LEN];
    if (fgets(pid_str, sizeof(pid_str), file) == NULL) {
        fclose(file);
        return -1;
    }
    
    fclose(file);
    return (pid_t)atoi(pid_str);
}

int is_process_running(pid_t pid) {
    return kill(pid, 0) == 0;
}

void cleanup() {
    if (is_running()) {
        unlink(LOCK_FILE);
    }
}

void signal_handler(int sig) {
    printf("\n%s: Señal recibida %d, limpiando...\n", PROGRAM_NAME, sig);
    
    char* window_id = get_window_id();
    if (window_id != NULL) {
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id);
        system(cmd);
        printf("%s: La inactividad del escritorio ya no está inhibida\n", PROGRAM_NAME);
    }
    
    cleanup();
    exit(0);
}

int daemon_main() {
    // Initialize X11
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Error: No se puede abrir el display X11\n");
        return 1;
    }
    
    // Create a dummy window
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    
    Window window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    if (window == None) {
        fprintf(stderr, "Error: No se puede crear la ventana X11\n");
        XCloseDisplay(display);
        return 1;
    }
    
    // Set window name
    XStoreName(display, window, "Wakefull");
    XFlush(display);
    
    // Convert window ID to hex string
    char window_id[MAX_ID_LEN];
    snprintf(window_id, sizeof(window_id), "0x%lx", window);
    
    // Save window ID to lock file
    save_window_id(window_id);
    
    // Execute xdg-screensaver suspend
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend %s", window_id);
    
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Error: Falló al suspender el salvapantallas\n");
        cleanup();
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        return 1;
    }
    
    // Setup signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // Keep X11 connection alive and wait
    while (is_running()) {
        sleep(1);
    }
    
    // Cleanup
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    return 0;
}

int show_status() {
    if (!is_running()) {
        printf("%s: No está funcionando\n", PROGRAM_NAME);
        return 0;
    }
    
    pid_t daemon_pid = get_daemon_pid();
    if (daemon_pid <= 0) {
        printf("%s: El archivo de bloqueo existe pero no se encontró el PID\n", PROGRAM_NAME);
        return 1;
    }
    
    if (is_process_running(daemon_pid)) {
        printf("%s: Funcionando (PID: %d)\n", PROGRAM_NAME, daemon_pid);
        printf("%s: La inactividad del escritorio está inhibida\n", PROGRAM_NAME);
        return 0;
    } else {
        printf("%s: Archivo de bloqueo obsoleto encontrado, daemon no está funcionando\n", PROGRAM_NAME);
        return 1;
    }
}

int start_wakefull() {
    if (is_running()) {
        pid_t daemon_pid = get_daemon_pid();
        if (daemon_pid > 0 && is_process_running(daemon_pid)) {
            printf("%s: Ya está funcionando (PID: %d)\n", PROGRAM_NAME, daemon_pid);
            return 1;
        } else {
            // Stale lock file, remove it
            cleanup();
        }
    }
    
    printf("%s: Iniciando daemon...\n", PROGRAM_NAME);
    
    // Fork to background
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: Falló al crear el daemon\n");
        return 1;
    }
    
    if (pid > 0) {
        // Parent process
        printf("%s: La inactividad del escritorio ahora está inhibida (daemon PID: %d)\n", PROGRAM_NAME, pid);
        printf("%s: Usa '%s --stop' para detener\n", PROGRAM_NAME, PROGRAM_NAME);
        return 0;
    }
    
    // Child process (daemon)
    setsid(); // Create new session
    
    // Close stdin, stdout, stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Run daemon
    return daemon_main();
}

int stop_wakefull() {
    if (!is_running()) {
        printf("%s: No está funcionando actualmente\n", PROGRAM_NAME);
        return 1;
    }
    
    char* window_id = get_window_id();
    if (window_id == NULL) {
        fprintf(stderr, "Error: No se puede leer el ID de ventana del archivo de bloqueo\n");
        return 1;
    }
    
    // Execute xdg-screensaver resume
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id);
    
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Advertencia: Falló al reanudar el salvapantallas\n");
    }
    
    // Clean up lock file
    cleanup();
    
    printf("%s: La inactividad del escritorio ya no está inhibida\n", PROGRAM_NAME);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }
    else if (strcmp(argv[1], "--start") == 0) {
        return start_wakefull();
    }
    else if (strcmp(argv[1], "--stop") == 0) {
        return stop_wakefull();
    }
    else if (strcmp(argv[1], "--status") == 0) {
        return show_status();
    }
    else {
        fprintf(stderr, "Error: Opción desconocida '%s'\n\n", argv[1]);
        print_usage();
        return 1;
    }
}