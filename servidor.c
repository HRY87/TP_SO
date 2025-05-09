#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ctype.h>

#define PUERTO 8080                 //Puerto a utilizar
#define MAX_CLIENTES 5              //Cantidad maxima de clientes permitidos
#define BUFFER_SIZE 1024           //Tamaño del buffer (Ojo con esto que puede haber overflow)
#define MAX_OPERACIONES 100       //Tamaño de operaciones permitidas
#define MAX_OPERACION_LENGTH 100 // Tamaño máximo para la operación almacenada

//Estructura para guardar las operaciones
typedef struct {
    char operacion[MAX_OPERACION_LENGTH];
    float resultado;
} Operacion;

//Use un buffer con memoria estatica (No se si usar memoria dinamica)
Operacion sumas[MAX_OPERACIONES];
Operacion restas[MAX_OPERACIONES];
Operacion multiplicaciones[MAX_OPERACIONES];
Operacion divisiones[MAX_OPERACIONES];
int contador_sumas = 0, contador_restas = 0, contador_multiplicaciones = 0, contador_divisiones = 0;

//Variables para el servidor
int servidor_socket;
pthread_mutex_t archivo_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int servidor_activo = 1;

//Funciones para el servidor
void* atender_cliente(void* arg);       
void* escuchar_enter(void* arg); 
float procesar_operacion(const char *operacion, char *operador_out);

void generar_archivos();


int main(){

    struct sockaddr_in servidor_addr, cliente_addr;
    pthread_t hilo_entrada;

    // Crea el socket
    if ((servidor_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    //Solo direcciones en IPv4
    servidor_addr.sin_family = AF_INET;
    //Permite que el servidor escuche en todas las interfaces de red disponibles en la máquina
    servidor_addr.sin_addr.s_addr = INADDR_ANY;
    //Establece el número de puerto en el que el servidor escuchará las conexiones entrantes.
    servidor_addr.sin_port = htons(PUERTO);

    if (bind(servidor_socket, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("Error al hacer bind");
        exit(EXIT_FAILURE);
    }

    if (listen(servidor_socket, MAX_CLIENTES) < 0) {
        perror("Error al escuchar");
        exit(EXIT_FAILURE);
    }

    printf("Servidor de cálculo iniciado. Presione Enter para finalizar.\n");

    //Crea el hilo entrada (servidor)
    pthread_create(&hilo_entrada, NULL, escuchar_enter, NULL);

    socklen_t cliente_len = sizeof(cliente_addr);
    while (servidor_activo) {

        int cliente_socket = accept(servidor_socket, (struct sockaddr *)&cliente_addr, &cliente_len);

        if (cliente_socket < 0) {
            if (servidor_activo) perror("Error al aceptar cliente");
            continue;
        }

        //Crear hilo cliente
        pthread_t hilo_cliente;
        int *pcliente = malloc(sizeof(int));
        *pcliente = cliente_socket;
        pthread_create(&hilo_cliente, NULL, atender_cliente, pcliente);
        pthread_detach(hilo_cliente);
    }

    generar_archivos(); // Generar archivos al finalizar
    close(servidor_socket); //Cerrar servidor (No olvidar)
    printf("Servidor finalizado.\n");
    return 0;
}

//Crea el hilo para el cliente y funcionamiento principal
void *atender_cliente(void *arg) 
{
    int cliente_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int leidos;

    while ((leidos = recv(cliente_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[leidos] = '\0';

        // Eliminar saltos de línea al final
        char *pos;
        if ((pos = strchr(buffer, '\n')) != NULL) *pos = '\0';
        if ((pos = strchr(buffer, '\r')) != NULL) *pos = '\0';

        // Si el cliente envía salir, termina la conexión
        if (strcmp(buffer, "salir") == 0) {
            break;
        }

        char operador;

        //Obtengo el operador y el resultado
        float resultado = procesar_operacion(buffer, &operador);

        char respuesta[BUFFER_SIZE];

        // Validar la operación
        if (operador == '?' || (operador == '/' && resultado == 0.0 && strstr(buffer, "/ 0") != NULL)) {
            snprintf(respuesta, sizeof(respuesta), "ERROR Operacion invalida o division por cero\n");
            send(cliente_socket, respuesta, strlen(respuesta), 0);
            continue;
        }

        snprintf(respuesta, sizeof(respuesta), "RESULTADO %.2f\n", resultado);
        send(cliente_socket, respuesta, strlen(respuesta), 0);

        //Dependiendo el operador, lo guardo en el buffer correspondiente (Revisar creo que se puede mejorar)
        pthread_mutex_lock(&archivo_mutex);
        if (operador == '+') {
            if (contador_sumas < MAX_OPERACIONES) {
                snprintf(sumas[contador_sumas].operacion, MAX_OPERACION_LENGTH, "%.92s = %.2f", buffer, resultado);
                sumas[contador_sumas].operacion[MAX_OPERACION_LENGTH - 1] = '\0'; // seguridad
                contador_sumas++;
            }
        }
        
        if (operador == '-') {
            if (contador_restas < MAX_OPERACIONES) {
                snprintf(restas[contador_restas].operacion, MAX_OPERACION_LENGTH, "%.92s = %.2f", buffer, resultado);
                restas[contador_restas].operacion[MAX_OPERACION_LENGTH - 1] = '\0';
                contador_restas++;
            }
        } 
        
        if (operador == '*') {
            if (contador_multiplicaciones < MAX_OPERACIONES) {
                snprintf(multiplicaciones[contador_multiplicaciones].operacion, MAX_OPERACION_LENGTH, "%.92s = %.2f", buffer, resultado);
                multiplicaciones[contador_multiplicaciones].operacion[MAX_OPERACION_LENGTH - 1] = '\0';
                contador_multiplicaciones++;
            }
        }

        if (operador == '/') {
            if (contador_divisiones < MAX_OPERACIONES) {
                snprintf(divisiones[contador_divisiones].operacion, MAX_OPERACION_LENGTH, "%.92s = %.2f", buffer, resultado);
                divisiones[contador_divisiones].operacion[MAX_OPERACION_LENGTH - 1] = '\0';
                contador_divisiones++;
            }
        }
        pthread_mutex_unlock(&archivo_mutex);
    }

    close(cliente_socket);
    return NULL;
}

float procesar_operacion(const char *operacion, char *operador_out) {

    float op1 = 0.0f, op2 = 0.0f;
    char operador;

    if (sscanf(operacion, "%f %c %f", &op1, &operador, &op2) != 3) {
        *operador_out = '?';
        return 0.0;
    }

    *operador_out = operador;

    switch (operador) {

        case '+': return op1 + op2;
        case '-': return op1 - op2;
        case '*': return op1 * op2;
        case '/': return (op2 != 0) ? op1 / op2 : 0.0; //Controlo si se divide por cero
        default: return 0.0;
    }
}


//Cuando se cierra el servidor, se genera los archivos
void generar_archivos() {

    pthread_mutex_lock(&archivo_mutex);

    FILE *f;

    f = fopen("sumas.txt", "w");
    if (f) {
        for (int i = 0; i < contador_sumas; i++) {
            fprintf(f, "%s\n", sumas[i].operacion);
        }
        fclose(f);
    } else {
        perror("Error abriendo sumas.txt");
    }

    f = fopen("restas.txt", "w");
    if (f) {
        for (int i = 0; i < contador_restas; i++) {
            fprintf(f, "%s\n", restas[i].operacion);
        }
        fclose(f);
    } else {
        perror("Error abriendo restas.txt");
    }

    f = fopen("multiplicaciones.txt", "w");
    if (f) {
        for (int i = 0; i < contador_multiplicaciones; i++) {
            fprintf(f, "%s\n", multiplicaciones[i].operacion);
        }
        fclose(f);
    } else {
        perror("Error abriendo multiplicaciones.txt");
    }

    f = fopen("divisiones.txt", "w");
    if (f) {
        for (int i = 0; i < contador_divisiones; i++) {
            fprintf(f, "%s\n", divisiones[i].operacion);
        }
        fclose(f);
    } else {
        perror("Error abriendo divisiones.txt");
    }

    pthread_mutex_unlock(&archivo_mutex);
}

void* escuchar_enter(void* arg) 
{
    (void)arg;
    getchar();  // Esperar Enter para detener el servidor
    servidor_activo = 0;
    shutdown(servidor_socket, SHUT_RDWR);
    close(servidor_socket);
    return NULL;
}
