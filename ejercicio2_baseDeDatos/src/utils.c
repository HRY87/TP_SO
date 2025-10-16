#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"
#include <stdarg.h>
#include <time.h>

// Debug logger (mantiene toda la información y snapshots)
static FILE *log_fp = NULL;
static char log_path[512] = "server_debug.log";

// Action logger (solo acciones cliente<->servidor)
static FILE *action_fp = NULL;
static char action_path[512] = "server.log";

// Inicializa logger debug (info detallada)
void init_logger(const char *path) {
    if (path && path[0] != '\0') {
        strncpy(log_path, path, sizeof(log_path)-1);
        log_path[sizeof(log_path)-1] = '\0';
    }
    log_fp = fopen(log_path, "a");
    if (!log_fp) {
        perror("No se pudo abrir debug log");
        log_fp = NULL;
    }
}

// Cierra logger debug
void close_logger() {
    if (log_fp) {
        fflush(log_fp);
        fclose(log_fp);
        log_fp = NULL;
    }
}

// Inicializa logger de acciones (server.log)
void init_action_logger(const char *path) {
    if (path && path[0] != '\0') {
        strncpy(action_path, path, sizeof(action_path)-1);
        action_path[sizeof(action_path)-1] = '\0';
    }
    action_fp = fopen(action_path, "a");
    if (!action_fp) {
        perror("No se pudo abrir action log");
        action_fp = NULL;
    }
}

// Cierra logger de acciones
void close_action_logger() {
    if (action_fp) {
        fflush(action_fp);
        fclose(action_fp);
        action_fp = NULL;
    }
}

// Escribe entrada de debug con timestamp
void log_msg(const char *fmt, ...) {
    if (!log_fp) return;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(log_fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_fp, fmt, ap);
    va_end(ap);
    fprintf(log_fp, "\n");
    fflush(log_fp);
}

// Escribe entrada de acción (sin tanto ruido) con timestamp
void log_action(const char *fmt, ...) {
    if (!action_fp) return;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(action_fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(action_fp, fmt, ap);
    va_end(ap);
    fprintf(action_fp, "\n");
    fflush(action_fp);
}

// Ejecuta comandos de monitoreo y vuelca salida al debug log (log_fp)
void log_monitoring_snapshot() {
    if (!log_fp) return;
    log_msg("=== MONITOREO: netstat -tulpn ===");
    FILE *p = popen("netstat -tulpn 2>/dev/null", "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof(line), p)) fputs(line, log_fp);
        pclose(p);
    }
    log_msg("=== MONITOREO: ss -tnlp ===");
    p = popen("ss -tnlp 2>/dev/null", "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof(line), p)) fputs(line, log_fp);
        pclose(p);
    }
    log_msg("=== MONITOREO: lsof -iTCP -sTCP:LISTEN -P -n ===");
    p = popen("lsof -iTCP -sTCP:LISTEN -P -n 2>/dev/null", "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof(line), p)) fputs(line, log_fp);
        pclose(p);
    }
    log_msg("=== MONITOREO: ps -eLf (primeras líneas) ===");
    p = popen("ps -eLf 2>/dev/null | sed -n '1,200p'", "r");
    if (p) {
        char line[512];
        while (fgets(line, sizeof(line), p)) fputs(line, log_fp);
        pclose(p);
    }
    fflush(log_fp);
}

// Envía un mensaje al socket del cliente
void enviar(int socket, const char *mensaje) {
    if (socket >= 0 && mensaje) send(socket, mensaje, strlen(mensaje), 0);
}

// Recibe un mensaje del socket del cliente
void recibir(int socket, char *buffer, size_t size) {
    if (!buffer || size == 0) return;
    memset(buffer, 0, size);
    recv(socket, buffer, size - 1, 0);
}

// Función para imprimir un mensaje de error y salir
void error(const char *mensaje) {
    perror(mensaje);
    exit(EXIT_FAILURE);
}

// Función para cerrar el socket
void cerrar_socket(int socket) {
    close(socket);
}