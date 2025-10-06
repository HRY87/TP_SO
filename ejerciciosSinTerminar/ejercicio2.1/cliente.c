// cliente.c - Cliente interactivo de productos
// Compilar: gcc -o cliente cliente.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define TAM_BUFFER 2048

void mostrar_menu() {
    printf("\n----- MENU PRODUCTOS -----\n");
    printf("1. Mostrar productos\n");
    printf("2. Iniciar transaccion\n");
    printf("3. Agregar producto\n");
    printf("4. Eliminar producto\n");
    printf("5. Modificar producto\n");
    printf("6. Confirmar transaccion\n");
    printf("7. Salir\n");
    printf("Seleccione opcion: ");
}

void enviar_comando(int sock, const char *comando) {
    send(sock, comando, strlen(comando), 0);

    char buffer[TAM_BUFFER];
    int n = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <IP> <PUERTO>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int puerto = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dir;
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    inet_aton(ip, &dir.sin_addr);

    if (connect(sock, (struct sockaddr*)&dir, sizeof(dir)) < 0) {
        perror("connect");
        exit(1);
    }

    printf("Conectado al servidor %s:%d\n", ip, puerto);

    int opcion;
    char entrada[TAM_BUFFER];

    do {
        mostrar_menu();
        scanf("%d", &opcion);
        getchar();

        switch(opcion) {
            case 1:
                enviar_comando(sock, "MOSTRAR");
                break;
            case 2:
                enviar_comando(sock, "BEGIN TRANSACTION");
                break;
            case 3:
                printf("Formato: id,descripcion,stock\nIngrese: ");
                fgets(entrada, sizeof(entrada), stdin);
                entrada[strcspn(entrada, "\n")] = '\0';
                char cmd1[TAM_BUFFER];
                snprintf(cmd1, sizeof(cmd1), "AGREGAR;%s", entrada);
                enviar_comando(sock, cmd1);
                break;
            case 4:
                printf("Ingrese id: ");
                fgets(entrada, sizeof(entrada), stdin);
                entrada[strcspn(entrada, "\n")] = '\0';
                char cmd2[TAM_BUFFER];
                snprintf(cmd2, sizeof(cmd2), "ELIMINAR;%s", entrada);
                enviar_comando(sock, cmd2);
                break;
            case 5:
                printf("Formato: id;descripcion;stock\nIngrese: ");
                fgets(entrada, sizeof(entrada), stdin);
                entrada[strcspn(entrada, "\n")] = '\0';
                char cmd3[TAM_BUFFER];
                snprintf(cmd3, sizeof(cmd3), "MODIFICAR;%s", entrada);
                enviar_comando(sock, cmd3);
                break;
            case 6:
                enviar_comando(sock, "COMMIT TRANSACTION");
                break;
            case 7:
                enviar_comando(sock, "SALIR");
                break;
            default:
                printf("Opcion invalida\n");
        }

    } while(opcion != 7);

    close(sock);
    return 0;
}
