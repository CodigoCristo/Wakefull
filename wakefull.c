/*
 * Wakefull - Mantén tu computadora despierta
 * 
 * Versión mejorada que previene tanto el protector de pantalla como el bloqueo automático
 * Compatible con XFCE4, GNOME, KDE y otros entornos de escritorio
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
#include <sys/wait.h>

#define PROGRAM_NAME "wakefull"
#define LOCK_FILE "/tmp/wakefull.lock"
#define MAX_CMD_LEN 512
#define MAX_ID_LEN 32
#define MAX_PID_LEN 16

// Variables globales para cleanup
static unsigned int dbus_cookie = 0;
static char window_id_global[MAX_ID_LEN] = {0};
static Display *display_global = NULL;
static Window window_global = None;

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
    
    // Skip dbus cookie line, if it exists
    char dbus_line[32];
    fgets(dbus_line, sizeof(dbus_line), file);
    
    fclose(file);
    
    // Remove newline if present
    int len = strlen(window_id);
    if (len > 0 && window_id[len-1] == '\n') {
        window_id[len-1] = '\0';
    }
    
    return window_id;
}

unsigned int get_dbus_cookie() {
    FILE *file = fopen(LOCK_FILE, "r");
    if (file == NULL) {
        return 0;
    }
    
    char line1[MAX_PID_LEN];
    char line2[MAX_ID_LEN];
    char dbus_line[32];
    
    // Skip PID and window ID lines
    fgets(line1, sizeof(line1), file);
    fgets(line2, sizeof(line2), file);
    
    if (fgets(dbus_line, sizeof(dbus_line), file) == NULL) {
        fclose(file);
        return 0;
    }
    
    fclose(file);
    return (unsigned int)atoi(dbus_line);
}

void save_session_data(const char* window_id, unsigned int dbus_cookie) {
    FILE *file = fopen(LOCK_FILE, "w");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot create lock file\n");
        exit(1);
    }
    
    fprintf(file, "%d\n%s\n%u\n", getpid(), window_id, dbus_cookie);
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

int execute_command(const char* cmd) {
    int result = system(cmd);
    return WEXITSTATUS(result);
}

unsigned int inhibit_dbus_power_management() {
    char cmd[MAX_CMD_LEN];
    FILE *pipe;
    char result[64];
    unsigned int cookie = 0;
    
    // Try to inhibit via D-Bus (works with most desktop environments)
    snprintf(cmd, sizeof(cmd), 
        "dbus-send --session --type=method_call --print-reply "
        "--dest=org.freedesktop.PowerManagement "
        "/org/freedesktop/PowerManagement/Inhibit "
        "org.freedesktop.PowerManagement.Inhibit.Inhibit "
        "string:'Wakefull' string:'Preventing screen lock and sleep' 2>/dev/null | "
        "grep -o 'uint32 [0-9]*' | cut -d' ' -f2");
    
    pipe = popen(cmd, "r");
    if (pipe != NULL) {
        if (fgets(result, sizeof(result), pipe) != NULL) {
            cookie = (unsigned int)atoi(result);
        }
        pclose(pipe);
    }
    
    // Also try gnome-session-inhibit if available
    if (cookie == 0) {
        snprintf(cmd, sizeof(cmd),
            "dbus-send --session --type=method_call --print-reply "
            "--dest=org.gnome.SessionManager "
            "/org/gnome/SessionManager "
            "org.gnome.SessionManager.Inhibit "
            "string:'Wakefull' uint32:0 string:'Preventing screen lock and sleep' uint32:12 2>/dev/null | "
            "grep -o 'uint32 [0-9]*' | cut -d' ' -f2");
            
        pipe = popen(cmd, "r");
        if (pipe != NULL) {
            if (fgets(result, sizeof(result), pipe) != NULL) {
                cookie = (unsigned int)atoi(result);
            }
            pclose(pipe);
        }
    }
    
    return cookie;
}

void uninhibit_dbus_power_management(unsigned int cookie) {
    char cmd[MAX_CMD_LEN];
    
    if (cookie == 0) return;
    
    // Try to uninhibit via standard PowerManagement interface
    snprintf(cmd, sizeof(cmd),
        "dbus-send --session --type=method_call "
        "--dest=org.freedesktop.PowerManagement "
        "/org/freedesktop/PowerManagement/Inhibit "
        "org.freedesktop.PowerManagement.Inhibit.UnInhibit "
        "uint32:%u 2>/dev/null", cookie);
    execute_command(cmd);
    
    // Also try gnome-session interface
    snprintf(cmd, sizeof(cmd),
        "dbus-send --session --type=method_call "
        "--dest=org.gnome.SessionManager "
        "/org/gnome/SessionManager "
        "org.gnome.SessionManager.Uninhibit "
        "uint32:%u 2>/dev/null", cookie);
    execute_command(cmd);
}

void disable_xfce4_lock() {
    // Temporarily disable XFCE4 lock screen
    execute_command("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/lock-screen-suspend-hibernate -s false 2>/dev/null");
    execute_command("xfconf-query -c xfce4-screensaver -p /lock/enabled -s false 2>/dev/null");
    execute_command("xfconf-query -c xfce4-session -p /general/LockCommand -s '' 2>/dev/null");
}

void restore_xfce4_lock() {
    // Restore XFCE4 lock screen settings
    execute_command("xfconf-query -c xfce4-power-manager -p /xfce4-power-manager/lock-screen-suspend-hibernate -s true 2>/dev/null");
    execute_command("xfconf-query -c xfce4-screensaver -p /lock/enabled -s true 2>/dev/null");
    execute_command("xfconf-query -c xfce4-session -p /general/LockCommand -s 'xflock4' 2>/dev/null");
}

void cleanup() {
    // Resume screensaver
    if (strlen(window_id_global) > 0) {
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id_global);
        execute_command(cmd);
    }
    
    // Uninhibit D-Bus power management
    if (dbus_cookie > 0) {
        uninhibit_dbus_power_management(dbus_cookie);
    }
    
    // Restore XFCE4 settings
    restore_xfce4_lock();
    
    // Clean up X11 resources
    if (window_global != None && display_global != NULL) {
        XDestroyWindow(display_global, window_global);
    }
    if (display_global != NULL) {
        XCloseDisplay(display_global);
    }
    
    // Remove lock file
    if (is_running()) {
        unlink(LOCK_FILE);
    }
}

void signal_handler(int sig) {
    printf("\n%s: Señal recibida %d, limpiando...\n", PROGRAM_NAME, sig);
    cleanup();
    printf("%s: La inactividad del escritorio ya no está inhibida\n", PROGRAM_NAME);
    exit(0);
}

int daemon_main() {
    // Initialize X11
    display_global = XOpenDisplay(NULL);
    if (display_global == NULL) {
        fprintf(stderr, "Error: No se puede abrir el display X11\n");
        return 1;
    }
    
    // Create a dummy window
    int screen = DefaultScreen(display_global);
    Window root = RootWindow(display_global, screen);
    
    window_global = XCreateSimpleWindow(display_global, root, 0, 0, 1, 1, 0, 0, 0);
    if (window_global == None) {
        fprintf(stderr, "Error: No se puede crear la ventana X11\n");
        XCloseDisplay(display_global);
        return 1;
    }
    
    // Set window name
    XStoreName(display_global, window_global, "Wakefull");
    XFlush(display_global);
    
    // Convert window ID to hex string
    snprintf(window_id_global, sizeof(window_id_global), "0x%lx", window_global);
    
    // Inhibit power management via D-Bus
    dbus_cookie = inhibit_dbus_power_management();
    
    // Save session data
    save_session_data(window_id_global, dbus_cookie);
    
    // Suspend screensaver
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend %s", window_id_global);
    
    int result = execute_command(cmd);
    if (result != 0) {
        fprintf(stderr, "Advertencia: Falló al suspender el salvapantallas\n");
    }
    
    // Disable XFCE4 lock temporarily
    disable_xfce4_lock();
    
    // Setup signal handlers for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // Keep running and periodically send activity signals
    while (is_running()) {
        // Send a fake input event every 30 seconds to prevent any timeout
        execute_command("xdotool key shift 2>/dev/null");
        sleep(30);
    }
    
    // Cleanup
    cleanup();
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
        printf("%s: La inactividad del escritorio y el bloqueo automático están inhibidos\n", PROGRAM_NAME);
        unsigned int cookie = get_dbus_cookie();
        if (cookie > 0) {
            printf("%s: D-Bus inhibition cookie: %u\n", PROGRAM_NAME, cookie);
        }
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
            unlink(LOCK_FILE);
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
        sleep(1); // Give daemon time to initialize
        printf("%s: La inactividad del escritorio y el bloqueo automático ahora están inhibidos (daemon PID: %d)\n", PROGRAM_NAME, pid);
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
    unsigned int cookie = get_dbus_cookie();
    
    if (window_id != NULL) {
        // Resume screensaver
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id);
        execute_command(cmd);
    }
    
    // Uninhibit D-Bus power management
    if (cookie > 0) {
        uninhibit_dbus_power_management(cookie);
    }
    
    // Restore XFCE4 settings
    restore_xfce4_lock();
    
    // Kill daemon process
    pid_t daemon_pid = get_daemon_pid();
    if (daemon_pid > 0 && is_process_running(daemon_pid)) {
        kill(daemon_pid, SIGTERM);
        
        // Wait a moment for graceful shutdown
        sleep(1);
        
        // Force kill if still running
        if (is_process_running(daemon_pid)) {
            kill(daemon_pid, SIGKILL);
        }
    }
    
    // Clean up lock file
    unlink(LOCK_FILE);
    
    printf("%s: La inactividad del escritorio y el bloqueo automático ya no están inhibidos\n", PROGRAM_NAME);
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