#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PUERTO 8080
#define BUFFER_SIZE 1024

int main() 
{
    int socket_cliente;
    struct sockaddr_in servidor_addr;
    char buffer[BUFFER_SIZE];

    // Crear socket
    if ((socket_cliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(PUERTO);
    servidor_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Localhost

    if (connect(socket_cliente, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("Error al conectar con el servidor");
        exit(EXIT_FAILURE);
    }

    printf("Conectado al servidor. Ingrese operaciones o 'salir' para terminar.\n");

    while (1) {
        printf("Operacion: ");
        fgets(buffer, BUFFER_SIZE, stdin);

        if (strncmp(buffer, "salir", 5) == 0)
            break;

        send(socket_cliente, buffer, strlen(buffer), 0);

        int leidos = recv(socket_cliente, buffer, BUFFER_SIZE - 1, 0);
        if (leidos <= 0) {
            printf("Desconectado del servidor.\n");
            break;
        }

        buffer[leidos] = '\0';
        printf("%s", buffer);
    }

    close(socket_cliente);
    return 0;
}