// cliente.c — versión mejorada y estable
// Corrige el problema de MOSTRAR: ya no hay que presionar Enter ni se mezclan los comandos.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>

#define BUFFER_SIZE 4096
#define TIMEOUT_MS 300 // milisegundos sin datos = fin de respuesta

void mostrar_menu() {
    printf("Comandos disponibles:\n");
    printf("  MOSTRAR\n");
    printf("  BUSCAR <texto>\n");
    printf("  AGREGAR <ID,Descripcion,Cantidad,Fecha,Hora,Generador>\n");
    printf("  MODIFICAR <ID>;<ID,Descripcion,Cantidad,Fecha,Hora,Generador>\n");
    printf("  ELIMINAR <ID>\n");
    printf("  BEGIN / COMMIT / ROLLBACK\n");
    printf("  FILTRO <campo>=<valor>\n");
    printf("  AYUDA\n");
    printf("  SALIR\n\n");
}

// elimina \r\n final
static void quitar_salto(char *s) {
    if (!s) return;
    s[strcspn(s, "\r\n")] = 0;
}

// función auxiliar: limpia stdin si quedó basura después de imprimir
static void limpiar_entrada() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void check_connection(void *arg) {
    int connection_fd = *(int *)arg;
    char temp_buffer[1];
    while (1) {
        int n = recv(connection_fd, temp_buffer, sizeof(temp_buffer), MSG_PEEK);
        if (n <= 0) {
            if (n < 0) {
                perror("\nError receiving data.");
            } else {
                printf("\nServer closed the connection.\n");
            }
            close(connection_fd);
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <IP> <PUERTO>\n", argv[0]);
        return 1;
    }
    const char *ip = argv[1];
    int puerto = atoi(argv[2]);
    pthread_t conn_thread;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(puerto);
    if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    // mensaje inicial si el servidor lo manda
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 1; tv.tv_usec = 0;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    if (select(sock + 1, &rfds, NULL, NULL, &tv) > 0) {
        char buffer[BUFFER_SIZE];
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            fputs(buffer, stdout);
        }
    }
    if (pthread_create(&conn_thread, NULL, (void *)check_connection, &sock) != 0) {
        perror("Error creating connection thread");
    }

    mostrar_menu();

    char buffer[BUFFER_SIZE];


    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        quitar_salto(buffer);
        if (strlen(buffer) == 0) continue; // evita líneas vacías

        // preparar comando
        size_t len = strlen(buffer);
        buffer[len++] = '\n';
        // si es AYUDA, mostrar menú
        if (strcasecmp(buffer, "AYUDA\n") == 0) {
            mostrar_menu();
            continue;
        }
        // enviar comando
        if (send(sock, buffer, len, 0) < 0) {
            perror("send");
            break;
        }
        
        if (strcasecmp(buffer, "SALIR\n") == 0) {
            printf("Desconectando...\n");
            break;
        }

        // Lectura no bloqueante: recibimos hasta que no haya datos por TIMEOUT_MS
        char respuesta[BUFFER_SIZE];
        int total = 0;
        int terminado = 0;
        while (!terminado) {
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = TIMEOUT_MS * 1000;
            int rv = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (rv < 0) {
                perror("select");
                terminado = 1;
                break;
            } else if (rv == 0) {
                // timeout: asumimos fin de respuesta
                terminado = 1;
                break;
            } else {
                if (FD_ISSET(sock, &rfds)) {
                    int n = recv(sock, respuesta, sizeof(respuesta) - 1, 0);
                    if (n <= 0) {
                        terminado = 1;
                        break;
                    }
                    respuesta[n] = '\0';
                    fputs(respuesta, stdout);
                    total += n;
                }
            }
        }

        // aseguramos que el prompt aparezca después de toda la salida
        printf("\n");
        fflush(stdout);

        // limpiamos stdin si quedó algún \n residual (por salidas largas)
        if (tcflush(STDIN_FILENO, TCIFLUSH) != 0) {
            limpiar_entrada();
        }
    }

    close(sock);
    return 0;
}
