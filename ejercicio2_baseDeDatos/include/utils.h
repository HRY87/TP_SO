#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

void init_logger(const char *path);
void close_logger(void);
void log_msg(const char *fmt, ...);
void log_monitoring_snapshot(void);

void init_action_logger(const char *path);
void close_action_logger(void);
void log_action(const char *fmt, ...);

void enviar(int socket, const char *mensaje);
void recibir(int socket, char *buffer, size_t size);
void error(const char *mensaje);
void cerrar_socket(int socket);

#endif // UTILS_H