/*
 * Wakefull - Keep your computer awake
 * 
 * Based on Caffeine functionality
 * 
 * Usage:
 *   wakefull --start    Start preventing desktop idleness
 *   wakefull --stop     Stop preventing desktop idleness
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
    printf("Usage: %s [OPTION]\n", PROGRAM_NAME);
    printf("Keep your computer awake by preventing desktop idleness.\n\n");
    printf("Options:\n");
    printf("  --start    Start preventing desktop idleness (daemon mode)\n");
    printf("  --stop     Stop preventing desktop idleness\n");
    printf("  --status   Show current status\n");
    printf("  --help     Display this help and exit\n");
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
    printf("\n%s: Received signal %d, cleaning up...\n", PROGRAM_NAME, sig);
    
    char* window_id = get_window_id();
    if (window_id != NULL) {
        char cmd[MAX_CMD_LEN];
        snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id);
        system(cmd);
        printf("%s: Desktop idleness is no longer inhibited\n", PROGRAM_NAME);
    }
    
    cleanup();
    exit(0);
}

int daemon_main() {
    // Initialize X11
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Error: Cannot open X11 display\n");
        return 1;
    }
    
    // Create a dummy window
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    
    Window window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    if (window == None) {
        fprintf(stderr, "Error: Cannot create X11 window\n");
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
        fprintf(stderr, "Error: Failed to suspend screensaver\n");
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
        printf("%s: Not running\n", PROGRAM_NAME);
        return 0;
    }
    
    pid_t daemon_pid = get_daemon_pid();
    if (daemon_pid <= 0) {
        printf("%s: Lock file exists but PID not found\n", PROGRAM_NAME);
        return 1;
    }
    
    if (is_process_running(daemon_pid)) {
        printf("%s: Running (PID: %d)\n", PROGRAM_NAME, daemon_pid);
        printf("%s: Desktop idleness is inhibited\n", PROGRAM_NAME);
        return 0;
    } else {
        printf("%s: Stale lock file found, daemon not running\n", PROGRAM_NAME);
        return 1;
    }
}

int start_wakefull() {
    if (is_running()) {
        pid_t daemon_pid = get_daemon_pid();
        if (daemon_pid > 0 && is_process_running(daemon_pid)) {
            printf("%s: Already running (PID: %d)\n", PROGRAM_NAME, daemon_pid);
            return 1;
        } else {
            // Stale lock file, remove it
            cleanup();
        }
    }
    
    printf("%s: Starting daemon...\n", PROGRAM_NAME);
    
    // Fork to background
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: Failed to fork daemon\n");
        return 1;
    }
    
    if (pid > 0) {
        // Parent process
        printf("%s: Desktop idleness is now inhibited (daemon PID: %d)\n", PROGRAM_NAME, pid);
        printf("%s: Use '%s --stop' to stop\n", PROGRAM_NAME, PROGRAM_NAME);
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
        printf("%s: Not currently running\n", PROGRAM_NAME);
        return 1;
    }
    
    char* window_id = get_window_id();
    if (window_id == NULL) {
        fprintf(stderr, "Error: Cannot read window ID from lock file\n");
        return 1;
    }
    
    // Execute xdg-screensaver resume
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "xdg-screensaver resume %s", window_id);
    
    int result = system(cmd);
    if (result != 0) {
        fprintf(stderr, "Warning: Failed to resume screensaver\n");
    }
    
    // Clean up lock file
    cleanup();
    
    printf("%s: Desktop idleness is no longer inhibited\n", PROGRAM_NAME);
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
        fprintf(stderr, "Error: Unknown option '%s'\n\n", argv[1]);
        print_usage();
        return 1;
    }
}