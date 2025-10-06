// ==========================
// Servidor de empleados
// ==========================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

// --------------------------
// Variables globales
// --------------------------
#define TAM_BUFFER 2048
#define ARCHIVO_BD "basedatos.csv"

int socket_servidor;
int clientes_activos = 0;
int max_clientes = 0;
pthread_mutex_t mutex_clientes = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_transaccion = PTHREAD_MUTEX_INITIALIZER;
int transaccion_activa = 0;  // 0 libre, 1 ocupada

// --------------------------
// Funciones auxiliares
// --------------------------

// Mostrar todos los empleados
void mostrar_empleados(int socket_cliente) {
    FILE *f = fopen(ARCHIVO_BD, "r");
    if (!f) {
        char msg[] = "No se pudo abrir la base de datos\n";
        send(socket_cliente, msg, strlen(msg), 0);
        return;
    }

    char linea[TAM_BUFFER];
    while (fgets(linea, sizeof(linea), f)) {
        send(socket_cliente, linea, strlen(linea), 0);
    }
    fclose(f);
}

// Agregar empleado
void agregar_empleado(char *datos, int socket_cliente) {
    FILE *f = fopen(ARCHIVO_BD, "a");
    if (!f) {
        char msg[] = "Error al abrir la base de datos\n";
        send(socket_cliente, msg, strlen(msg), 0);
        return;
    }
    fprintf(f, "%s\n", datos);  // datos ya vienen en formato CSV
    fclose(f);
    char msg[] = "Empleado agregado correctamente\n";
    send(socket_cliente, msg, strlen(msg), 0);
}

// Eliminar empleado por id
void eliminar_empleado(char *id, int socket_cliente) {
    FILE *f = fopen(ARCHIVO_BD, "r");
    FILE *temp = fopen("temp.csv", "w");
    if (!f || !temp) {
        char msg[] = "Error al procesar archivos\n";
        send(socket_cliente, msg, strlen(msg), 0);
        return;
    }

    char linea[TAM_BUFFER];
    int eliminado = 0;
    while (fgets(linea, sizeof(linea), f)) {
        char copia[TAM_BUFFER];
        strcpy(copia, linea);
        char *token = strtok(copia, ",");
        if (token && strcmp(token, id) == 0) {
            eliminado = 1;
            continue;  // no copiar este registro
        }
        fputs(linea, temp);
    }

    fclose(f);
    fclose(temp);
    remove(ARCHIVO_BD);
    rename("temp.csv", ARCHIVO_BD);

    if (eliminado)
        send(socket_cliente, "Empleado eliminado\n", 20, 0);
    else
        send(socket_cliente, "No se encontro empleado con ese ID\n", 35, 0);
}

// Modificar empleado
void modificar_empleado(char *datos, int socket_cliente) {
    // datos: id,nombre,apellido,puesto,salario
    char *id = strtok(datos, ";");
    char *nombre = strtok(NULL, ";");
    char *apellido = strtok(NULL, ";");
    char *puesto = strtok(NULL, ";");
    char *salario = strtok(NULL, ";");

    if (!id || !nombre || !apellido || !puesto || !salario) {
        send(socket_cliente, "Formato invalido\n", 17, 0);
        return;
    }

    FILE *f = fopen(ARCHIVO_BD, "r");
    FILE *temp = fopen("temp.csv", "w");
    if (!f || !temp) {
        send(socket_cliente, "Error al procesar archivos\n", 27, 0);
        return;
    }

    char linea[TAM_BUFFER];
    int modificado = 0;
    while (fgets(linea, sizeof(linea), f)) {
        char copia[TAM_BUFFER];
        strcpy(copia, linea);
        char *tok = strtok(copia, ",");
        if (tok && strcmp(tok, id) == 0) {
            fprintf(temp, "%s,%s,%s,%s,%s\n", id, nombre, apellido, puesto, salario);
            modificado = 1;
        } else {
            fputs(linea, temp);
        }
    }

    fclose(f);
    fclose(temp);
    remove(ARCHIVO_BD);
    rename("temp.csv", ARCHIVO_BD);

    if (modificado)
        send(socket_cliente, "Empleado modificado\n", 21, 0);
    else
        send(socket_cliente, "No se encontro empleado con ese ID\n", 35, 0);
}

// --------------------------
// Hilo para cliente
// --------------------------
void *atender_cliente(void *arg) {
    int socket_cliente = *(int *)arg;
    free(arg);

    char buffer[TAM_BUFFER];
    int salir = 0;

    while (!salir) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(socket_cliente, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;

        buffer[strcspn(buffer, "\n")] = 0; // limpiar salto

        if (strcmp(buffer, "SALIR") == 0) {
            send(socket_cliente, "Adios\n", 6, 0);
            salir = 1;
        }
        else if (strcmp(buffer, "MOSTRAR") == 0) {
            mostrar_empleados(socket_cliente);
        }
        else if (strcmp(buffer, "BEGIN TRANSACTION") == 0) {
            if (pthread_mutex_trylock(&mutex_transaccion) == 0) {
                transaccion_activa = 1;
                send(socket_cliente, "Transaccion iniciada\n", 22, 0);
            } else {
                send(socket_cliente, "Error: otra transaccion activa\n", 31, 0);
            }
        }
        else if (strncmp(buffer, "AGREGAR;", 8) == 0) {
            if (transaccion_activa)
                agregar_empleado(buffer + 8, socket_cliente);
            else
                send(socket_cliente, "Error: debe iniciar transaccion\n", 32, 0);
        }
        else if (strncmp(buffer, "ELIMINAR;", 9) == 0) {
            if (transaccion_activa)
                eliminar_empleado(buffer + 9, socket_cliente);
            else
                send(socket_cliente, "Error: debe iniciar transaccion\n", 32, 0);
        }
        else if (strncmp(buffer, "MODIFICAR;", 10) == 0) {
            if (transaccion_activa)
                modificar_empleado(buffer + 10, socket_cliente);
            else
                send(socket_cliente, "Error: debe iniciar transaccion\n", 32, 0);
        }
        else if (strcmp(buffer, "COMMIT TRANSACTION") == 0) {
            if (transaccion_activa) {
                transaccion_activa = 0;
                pthread_mutex_unlock(&mutex_transaccion);
                send(socket_cliente, "Transaccion confirmada\n", 24, 0);
            } else {
                send(socket_cliente, "Error: no hay transaccion activa\n", 33, 0);
            }
        }
        else {
            send(socket_cliente, "Comando no reconocido\n", 23, 0);
        }
    }

    close(socket_cliente);

    pthread_mutex_lock(&mutex_clientes);
    clientes_activos--;
    pthread_mutex_unlock(&mutex_clientes);

    return NULL;
}

// --------------------------
// Main del servidor
// --------------------------
int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Uso: %s <IP> <PUERTO> <MAX_CLIENTES> <MAX_ESPERA>\n", argv[0]);
        exit(1);
    }

    char *ip = argv[1];
    int puerto = atoi(argv[2]);
    max_clientes = atoi(argv[3]);
    int max_espera = atoi(argv[4]);

    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dir;
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    inet_aton(ip, &dir.sin_addr);

    bind(socket_servidor, (struct sockaddr*)&dir, sizeof(dir));
    listen(socket_servidor, max_espera);

    printf("Servidor escuchando en %s:%d\n", ip, puerto);

    while (1) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int *sock_cliente = malloc(sizeof(int));
        *sock_cliente = accept(socket_servidor, (struct sockaddr*)&cli, &len);

        pthread_mutex_lock(&mutex_clientes);
        if (clientes_activos >= max_clientes) {
            printf("Servidor lleno, rechazando cliente...\n");
            close(*sock_cliente);
            free(sock_cliente);
            pthread_mutex_unlock(&mutex_clientes);
            continue;
        }
        clientes_activos++;
        pthread_mutex_unlock(&mutex_clientes);

        pthread_t th;
        pthread_create(&th, NULL, atender_cliente, sock_cliente);
        pthread_detach(th);
    }

    close(socket_servidor);
    return 0;
}
