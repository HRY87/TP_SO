#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdatomic.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include "db.h"
#include "transaction.h"
#include "utils.h"

#define BUFFER_SIZE 1024

// ====== Variables globales y sincronización ======
pthread_mutex_t mutex_archivo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_transaccion = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;

int transaccion_activa = 0;
pthread_t trans_owner;
atomic_int clientes_activos = 0;

int MAX_CLIENTES = 5;
int BACKLOG = 10;
uint8_t FOREGROUND = 0;
char CSV_PATH[512] = "data/productos.csv";
char LOG_PATH[512] = "server.log";


// ====== Prototipos ======
void *manejador_cliente(void *arg);
void cerrar_servidor(int signo);
void liberar_transaccion_si_propietario(pthread_t self);

// ====== Función principal ======
int main(int argc, char *argv[]) {
    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion;
    socklen_t addrlen = sizeof(direccion);

    if (argc < 3) {
        fprintf(stderr,
            "Uso: %s <IP_O_HOST> <PUERTO> [MAX_CLIENTES] [BACKLOG] [CSV_PATH] [LOG_PATH]\n"
            "Ejemplo: %s 8080 10 20 data/productos.csv server.log\n",
            argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }
     // Ejecutar en primer plano
    
    //const char *ip = argv[1];
    int puerto = atoi(argv[1]);
    if (argc >= 3) MAX_CLIENTES = atoi(argv[2]);
    if (argc >= 4) BACKLOG = atoi(argv[3]);
    if (argc >= 5) strncpy(CSV_PATH, argv[4], sizeof(CSV_PATH) - 1);
    if (argc >= 6) strncpy(LOG_PATH, argv[5], sizeof(LOG_PATH) - 1);
    if (argc >= 7) FOREGROUND = atoi(argv[6]);
    
    if (MAX_CLIENTES <= 0) MAX_CLIENTES = 5;
    if (BACKLOG <= 0) BACKLOG = 10;

    // Iniciar loggers
    init_logger("server_debug.log", FOREGROUND);
    init_action_logger(LOG_PATH, FOREGROUND);

    log_msg("Servidor iniciando en puerto %d (MAX_CLIENTES=%d, BACKLOG=%d, CSV=%s)",
            puerto, MAX_CLIENTES, BACKLOG, CSV_PATH);

    // Sincronizar ruta de DB con db.c
    strncpy(ARCHIVO_DB, CSV_PATH, sizeof(ARCHIVO_DB) - 1);
    ARCHIVO_DB[sizeof(ARCHIVO_DB) - 1] = '\0';

    // ===== Crear socket =====
    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd == -1) {
        perror("❌ Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Permitir reutilizar el puerto inmediatamente tras reinicios
    int opt = 1;
    if (setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("⚠️  Advertencia: setsockopt(SO_REUSEADDR)");
    }

    // ===== Configurar dirección =====
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    direccion.sin_addr.s_addr = htonl(INADDR_ANY);

    // ===== Bind =====
    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        fprintf(stderr, "❌ Error en bind(%d): %s\n", puerto, strerror(errno));
        log_msg("Error en bind(%d): %s", puerto, strerror(errno));
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    // ===== Listen =====
    if (listen(servidor_fd, BACKLOG) < 0) {
        perror("❌ Error en listen");
        log_msg("Error en listen");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    printf("✅ Servidor iniciado en puerto %d\n", puerto);
    log_msg("Servidor iniciado en puerto %d", puerto);

    // Manejar señal Ctrl+C
    signal(SIGINT, cerrar_servidor);

    // ===== Bucle principal =====
    while (1) {
        nuevo_socket = accept(servidor_fd, (struct sockaddr *)&direccion, &addrlen);
        if (nuevo_socket < 0) {
            perror("⚠️  Error en accept");
            log_msg("Error en accept");
            continue;
        }

        pthread_mutex_lock(&mutex_clientes);
        if (clientes_activos >= MAX_CLIENTES) {
            pthread_mutex_unlock(&mutex_clientes);
            enviar(nuevo_socket, "Servidor ocupado. Reintente más tarde.\n");
            close(nuevo_socket);
            log_action("Cliente rechazado: máximo de clientes alcanzado (%d).", MAX_CLIENTES);
            continue;
        }
        clientes_activos++;
        pthread_mutex_unlock(&mutex_clientes);

        log_action("Cliente conectado (socket=%d). Clientes activos=%d", nuevo_socket, clientes_activos);

        int *p_sock = malloc(sizeof(int));
        *p_sock = nuevo_socket;

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, manejador_cliente, p_sock) != 0) {
            perror("Error creando hilo");
            log_msg("Error creando hilo para socket=%d", nuevo_socket);
            free(p_sock);
            close(nuevo_socket);
            pthread_mutex_lock(&mutex_clientes);
            clientes_activos--;
            pthread_mutex_unlock(&mutex_clientes);
            continue;
        }
        pthread_detach(hilo);
    }

    close(servidor_fd);
    close_logger();
    return 0;
}

// ===== Manejador de clientes =====
void *manejador_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    free(arg);
    int my_turn = 0;

    char buffer[BUFFER_SIZE];
    enviar(socket_cliente, "📡 Conectado al servidor de base de datos.\n");

    pthread_t self = pthread_self();

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            log_action("Cliente (socket=%d) desconectado inesperadamente.", socket_cliente);
            break;
        }

        buffer[bytes] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;
        log_action("Recibido de socket=%d: %s", socket_cliente, buffer);

        // Comando en mayúsculas
        char cmd[32] = {0};
        sscanf(buffer, "%31s", cmd);
        for (int i = 0; cmd[i]; ++i) cmd[i] = toupper(cmd[i]);

        if (strncmp(cmd, "SALIR", 5) == 0) {
            enviar(socket_cliente, "👋 Desconectando...\n");
            break;
        }

        // Verificar bloqueo por transacción activa
        if (strcmp(cmd, "BEGIN") == 0) {
            pthread_mutex_lock(&mutex_transaccion);
            if (transaccion_activa) {
                enviar(socket_cliente, "❌ Ya existe una transacción activa.\n");
                if(!pthread_equal(trans_owner, self)) {
                    enviar(socket_cliente, "⚠️  Transacción activa por otro cliente. Reintente más tarde.\n");
                }
                pthread_mutex_unlock(&mutex_transaccion);
                continue;
            }else {
                transaccion_activa = 1;
                trans_owner = self;
                my_turn = 1;
                enviar(socket_cliente, "🚀 Transacción iniciada.\n");
            }
            pthread_mutex_unlock(&mutex_transaccion);
            continue;
        }

        if (!transaccion_activa || !my_turn) {
         enviar(socket_cliente, "Para comenzar una transacción, use el comando BEGIN.\n");
         continue;
        }else if (my_turn) {
        
            // ===== Procesar comandos =====
            if (strncmp(cmd, "MOSTRAR", 7) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                mostrar_registros(socket_cliente);
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "BUSCAR", 6) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                buscar_registro(socket_cliente, buffer + 7);
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "FILTRO", 6) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                filtrar_generador(socket_cliente, buffer + 7);
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "AGREGAR", 7) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                if (agregar_registro(buffer + 8) == 0) {
                    enviar(socket_cliente, "✅ Registro agregado correctamente.\n");
                } else {
                    enviar(socket_cliente, "❌ Error al agregar registro.\n");
                }
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "MODIFICAR", 9) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                if (modificar_registro(socket_cliente, buffer + 10) == 0) {
                    enviar(socket_cliente, "✅ Registro modificado correctamente.\n");
                } else {
                    enviar(socket_cliente, "❌ Registro no encontrado para modificar.\n");
                }
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "ELIMINAR", 8) == 0) {
                pthread_mutex_lock(&mutex_archivo);
                if (eliminar_registro(buffer + 9) == 0) {
                    enviar(socket_cliente, "✅ Registro eliminado correctamente.\n");
                } else {
                    enviar(socket_cliente, "❌ Registro no encontrado para eliminar.\n");
                }
                pthread_mutex_unlock(&mutex_archivo);
            }
            else if (strncmp(cmd, "COMMIT", 6) == 0) {
                pthread_mutex_lock(&mutex_transaccion);
                if (!my_turn) {
                    enviar(socket_cliente, "❌ No hay transacción activa o no es propietario.\n");
                } else {
                    transaccion_activa = 0;
                    my_turn = 0;
                    trans_owner = 0;
                    if (commit_temp() == 0)
                        enviar(socket_cliente, "✅ Transacción confirmada (COMMIT).\n");
                    else
                        enviar(socket_cliente, "⚠️  Error al confirmar transacción.\n");
                }
                pthread_mutex_unlock(&mutex_transaccion);
            }
            else if (strncmp(cmd, "ROLLBACK", 8) == 0) {
                pthread_mutex_lock(&mutex_transaccion);
                if (!my_turn) {
                    enviar(socket_cliente, "❌ No hay transacción activa o no es propietario.\n");
                } else {
                    if(rollback_transaccion())
                        enviar(socket_cliente, "⚠️  Error al revertir transacción.\n");
                    else
                        enviar(socket_cliente, "↩️  Transacción revertida (ROLLBACK).\n");
                }
                pthread_mutex_unlock(&mutex_transaccion);
            }
            else {
                enviar(socket_cliente, "❓ Comando no reconocido.\n");
            }
        }
    }

    liberar_transaccion_si_propietario(self);
    close(socket_cliente);

    pthread_mutex_lock(&mutex_clientes);
    clientes_activos--;
    pthread_mutex_unlock(&mutex_clientes);

    log_action("Cliente socket=%d desconectado. Clientes activos=%d", socket_cliente, clientes_activos);
    return NULL;
}

// ===== Limpieza de transacciones si cliente muere =====
void liberar_transaccion_si_propietario(pthread_t self) {
    pthread_mutex_lock(&mutex_transaccion);
    if (transaccion_activa && pthread_equal(trans_owner, self)) {
        transaccion_activa = 0;
        rollback_transaccion();
        log_msg("⚠️  Transacción liberada automáticamente por desconexión del cliente propietario.");
    }
    pthread_mutex_unlock(&mutex_transaccion);
}

// ===== Cierre ordenado del servidor =====
void cerrar_servidor(int signo) {
    log_action("🛑 Señal %d recibida. Cerrando servidor y liberando recursos...", signo);
    rollback_transaccion();
    close_action_logger();
    close_logger();
    printf("\nServidor detenido correctamente.\n");
    exit(0);
}
