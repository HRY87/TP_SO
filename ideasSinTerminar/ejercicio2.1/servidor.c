// servidor.c - Base de datos de productos
// Compilar: gcc -o servidor servidor.c -pthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define TAM_BUFFER 2048
#define ARCHIVO "productos.csv"

pthread_mutex_t mutex_bd = PTHREAD_MUTEX_INITIALIZER;
int transaccion_activa = 0;

void enviar(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

void mostrar_productos(int sock) {
    FILE *f = fopen(ARCHIVO, "r");
    if (!f) {
        enviar(sock, "No se pudo abrir la base de datos\n");
        return;
    }
    char linea[TAM_BUFFER];
    while (fgets(linea, sizeof(linea), f)) {
        enviar(sock, linea);
    }
    fclose(f);
}

void agregar_producto(const char *datos, int sock) {
    FILE *f = fopen(ARCHIVO, "a");
    if (!f) { enviar(sock, "Error abriendo archivo\n"); return; }
    fprintf(f, "%s\n", datos);
    fclose(f);
    enviar(sock, "Producto agregado correctamente\n");
}

void eliminar_producto(const char *id, int sock) {
    FILE *f = fopen(ARCHIVO, "r");
    FILE *tmp = fopen("tmp.csv", "w");
    if (!f || !tmp) { enviar(sock, "Error procesando archivo\n"); return; }

    char linea[TAM_BUFFER];
    int eliminado = 0;
    while (fgets(linea, sizeof(linea), f)) {
        char idlinea[50];
        sscanf(linea, "%[^,]", idlinea);
        if (strcmp(idlinea, id) == 0) {
            eliminado = 1;
            continue;
        }
        fputs(linea, tmp);
    }
    fclose(f); fclose(tmp);
    rename("tmp.csv", ARCHIVO);

    if (eliminado) enviar(sock, "Producto eliminado\n");
    else enviar(sock, "ID no encontrado\n");
}

void modificar_producto(const char *datos, int sock) {
    // datos formato: id;descripcion;stock
    char copia[TAM_BUFFER];
    strcpy(copia, datos);
    char *id = strtok(copia, ";");
    char *desc = strtok(NULL, ";");
    char *stock = strtok(NULL, ";");

    if (!id || !desc || !stock) {
        enviar(sock, "Formato inválido\n");
        return;
    }

    FILE *f = fopen(ARCHIVO, "r");
    FILE *tmp = fopen("tmp.csv", "w");
    if (!f || !tmp) { enviar(sock, "Error procesando archivo\n"); return; }

    char linea[TAM_BUFFER];
    int modificado = 0;
    while (fgets(linea, sizeof(linea), f)) {
        char idlinea[50];
        sscanf(linea, "%[^,]", idlinea);
        if (strcmp(idlinea, id) == 0) {
            fprintf(tmp, "%s,%s,%s\n", id, desc, stock);
            modificado = 1;
        } else {
            fputs(linea, tmp);
        }
    }
    fclose(f); fclose(tmp);
    rename("tmp.csv", ARCHIVO);

    if (modificado) enviar(sock, "Producto modificado\n");
    else enviar(sock, "ID no encontrado\n");
}

void *manejar_cliente(void *arg) {
    int sock = *(int*)arg;
    free(arg);

    char buffer[TAM_BUFFER];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int n = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;

        buffer[n] = '\0';

        if (strncmp(buffer, "MOSTRAR", 7) == 0) {
            if (transaccion_activa) {
                enviar(sock, "Error: transacción activa\n");
            } else {
                mostrar_productos(sock);
            }
        }
        else if (strncmp(buffer, "BEGIN TRANSACTION", 17) == 0) {
            if (transaccion_activa) enviar(sock, "Error: otra transacción activa\n");
            else {
                transaccion_activa = 1;
                enviar(sock, "Transacción iniciada\n");
            }
        }
        else if (strncmp(buffer, "COMMIT TRANSACTION", 18) == 0) {
            if (transaccion_activa) {
                transaccion_activa = 0;
                enviar(sock, "Transacción confirmada\n");
            } else {
                enviar(sock, "No hay transacción activa\n");
            }
        }
        else if (strncmp(buffer, "AGREGAR;", 8) == 0) {
            if (!transaccion_activa) enviar(sock, "Debe iniciar transacción\n");
            else agregar_producto(buffer+8, sock);
        }
        else if (strncmp(buffer, "ELIMINAR;", 9) == 0) {
            if (!transaccion_activa) enviar(sock, "Debe iniciar transacción\n");
            else eliminar_producto(buffer+9, sock);
        }
        else if (strncmp(buffer, "MODIFICAR;", 10) == 0) {
            if (!transaccion_activa) enviar(sock, "Debe iniciar transacción\n");
            else modificar_producto(buffer+10, sock);
        }
        else if (strncmp(buffer, "SALIR", 5) == 0) {
            enviar(sock, "Adiós\n");
            break;
        }
        else {
            enviar(sock, "Comando no reconocido\n");
        }
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <IP> <PUERTO>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int puerto = atoi(argv[2]);

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); exit(1); }

    struct sockaddr_in dir;
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    inet_aton(ip, &dir.sin_addr);

    if (bind(server, (struct sockaddr*)&dir, sizeof(dir)) < 0) {
        perror("bind");
        exit(1);
    }
    listen(server, 5);
    printf("Servidor escuchando en %s:%d\n", ip, puerto);

    while (1) {
        struct sockaddr_in cli;
        socklen_t l = sizeof(cli);
        int *nuevo_sock = malloc(sizeof(int));
        *nuevo_sock = accept(server, (struct sockaddr*)&cli, &l);
        if (*nuevo_sock < 0) { perror("accept"); free(nuevo_sock); continue; }

        pthread_t hilo;
        pthread_create(&hilo, NULL, manejar_cliente, nuevo_sock);
        pthread_detach(hilo);
    }

    close(server);
    return 0;
}
