// ==========================
// Cliente interactivo
// ==========================

// Compilar: gcc -o cliente cliente.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>

#define TAM_BUFFER 2048

void mostrar_menu() {
    printf("\n----- MENU CLIENTE -----\n");
    printf("1. Mostrar empleados\n");
    printf("2. Iniciar transaccion\n");
    printf("3. Agregar empleado\n");
    printf("4. Eliminar empleado\n");
    printf("5. Modificar empleado\n");
    printf("6. Confirmar transaccion\n");
    printf("7. Salir\n");
    printf("Seleccione opcion: ");
}

// Enviar comando y leer respuesta (lee todo lo que llegue en ventanas de 200ms)
void enviar_comando(int sock, const char *comando) {
    if (!comando) return;

    ssize_t s = send(sock, comando, strlen(comando), 0);
    if (s < 0) {
        perror("Error enviando comando");
        return;
    }

    // Ahora leemos lo que el servidor envíe: usamos select() con timeout corto
    char buffer[TAM_BUFFER];
    fd_set rfds;
    struct timeval tv;

    // Esperamos y leemos repetidamente mientras haya datos cada 200ms
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200 ms

        int sel = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (sel < 0) {
            perror("select");
            break;
        } else if (sel == 0) {
            // timeout: no hay más datos por el momento
            break;
        } else {
            if (FD_ISSET(sock, &rfds)) {
                ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (n <= 0) {
                    if (n == 0) printf("Servidor cerró la conexión\n");
                    else perror("recv");
                    exit(0);
                }
                buffer[n] = '\0';
                printf("%s", buffer);
                // seguirá el while para ver si llega más data en la próxima ventana
            }
        }
    }
    printf("\n"); // para separación visual
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <IP> <PUERTO>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int puerto = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in dir;
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    if (inet_aton(ip, &dir.sin_addr) == 0) {
        fprintf(stderr, "IP invalida\n");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr*)&dir, sizeof(dir)) < 0) {
        perror("Error al conectar");
        close(sock);
        exit(1);
    }

    printf("Conectado al servidor %s:%d\n", ip, puerto);

    int opcion;
    char entrada[TAM_BUFFER];

    do {
        mostrar_menu();
        if (scanf("%d", &opcion) != 1) {
            // entrada inválida
            while (getchar() != '\n'); // limpiar buffer stdin
            opcion = -1;
            continue;
        }
        getchar(); // consumir '\n'

        switch (opcion) {
            case 1:
                enviar_comando(sock, "MOSTRAR");
                break;

            case 2:
                enviar_comando(sock, "BEGIN TRANSACTION");
                break;

            case 3: { // AGREGAR: solicitamos datos (CSV: id,nombre,apellido,puesto,salario)
                printf("Ingrese datos (id,nombre,apellido,puesto,salario): ");
                if (!fgets(entrada, sizeof(entrada), stdin)) break;
                entrada[strcspn(entrada, "\n")] = '\0';

                size_t need = strlen(entrada) + 16;
                char *comando = malloc(need);
                if (!comando) { perror("malloc"); break; }
                snprintf(comando, need, "AGREGAR;%s", entrada);
                enviar_comando(sock, comando);
                free(comando);
                break;
            }

            case 4: { // ELIMINAR;id
                printf("Ingrese id del empleado a eliminar: ");
                if (!fgets(entrada, sizeof(entrada), stdin)) break;
                entrada[strcspn(entrada, "\n")] = '\0';

                size_t need = strlen(entrada) + 12;
                char *comando = malloc(need);
                if (!comando) { perror("malloc"); break; }
                snprintf(comando, need, "ELIMINAR;%s", entrada);
                enviar_comando(sock, comando);
                free(comando);
                break;
            }

            case 5: { // MODIFICAR;id;nombre;apellido;puesto;salario  (pedimos con ';')
                printf("Ingrese datos (id;nombre;apellido;puesto;salario): ");
                if (!fgets(entrada, sizeof(entrada), stdin)) break;
                entrada[strcspn(entrada, "\n")] = '\0';

                size_t need = strlen(entrada) + 16;
                char *comando = malloc(need);
                if (!comando) { perror("malloc"); break; }
                snprintf(comando, need, "MODIFICAR;%s", entrada);
                enviar_comando(sock, comando);
                free(comando);
                break;
            }

            case 6:
                enviar_comando(sock, "COMMIT TRANSACTION");
                break;

            case 7:
                enviar_comando(sock, "SALIR");
                break;

            default:
                printf("Opción inválida\n");
                break;
        }

    } while (opcion != 7);

    close(sock);
    return 0;
}
