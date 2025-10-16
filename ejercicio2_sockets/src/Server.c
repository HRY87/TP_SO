//general includes
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

//sockets includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

//signal and error include
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

//defines and constants
#define DB_PATH "./db/productos.csv"
#define MAXHOSTNAME 128
#define PORT 13000
#define CONN_LIMIT 5
#define BACKLOG 5
#define BUFFER_SIZE 1024
#define TIMEOUT 20
#define SERVER_MESSAGE  "Welcome to the server!\n To begin a transaction write BEGIN TRANSACTION"        \
                        " to get exclusive access\n to the Database. Once you gain access you can"      \
                        " use these commands:\n\nGET <key>\nSET <key> <value>\nADD <registry>\nDELETE"    \
                        " <key>\nTo finish the transaction write COMMIT TRANSACTION\n\n"

int establish(const char*, u_int16_t);
void zombie_catcher(int);
void database_transaction(int, int, int);
void process_connection(int, int, int);

sem_t connection_sem;
sem_t db_sem;

//Funcion que maneja la creacion, binding y activacion/escucha del socket del servidor.
int establish(const char *ip_address ,u_int16_t port_number) {

    int server_fd;
    struct sockaddr_in server;
    char hostbuffer[MAXHOSTNAME];
    int hostname;
    struct hostent *host;
    
    //seteo en zeros el struct de server
    memset(&server, 0, sizeof(struct sockaddr_in));
    hostname = gethostname(hostbuffer, MAXHOSTNAME);
    if (hostname == -1) {
        perror("Couldn't get hostname!");
        return -1;
    }
    host = gethostbyname(hostbuffer);
    if (host == NULL) {
        perror("Couldn't get host by name!");
        return -1;
    }

    printf("Starting server...\n");
    printf("Server Hostname: %s - %d\n", hostbuffer, hostname);
    for (int i = 0; host->h_addr_list[i] != NULL; i++) {
        printf("Server Address %d: %s\n", i + 1, inet_ntoa(*((struct in_addr*) host->h_addr_list[i])));
    }
    printf("Server Port: %d\n\n", port_number);

    //creo el file descriptor del socket del servidor.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Couldn't create socket!");
        return -1;
    }

    //configuracion de tipo, puerto e ip del servidor segun el host que lo ejecuta.
    server.sin_family = AF_INET;
    server.sin_port = htons(port_number);
    server.sin_addr.s_addr = INADDR_ANY; //inet_addr(ip_address);
    memset(&(server.sin_zero), '\0', 8);

    //relacionar el servidor con el fd del socket.
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
        perror("Couldn't Bind socket!");
        close(server_fd);
        return -1;
    }

    //el socket queda escuchando escuchando tantas conexiones como "BACKLOG" lo indique.
    if(listen(server_fd, BACKLOG) < 0) {
        perror("Can't listen on the socket!");
        close(server_fd);
        return -1;
    };
    return server_fd;

}

void zombie_catcher(int sig) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void database_transaction(int client_socket, int session_id, int db_fd) {
    char buffer[BUFFER_SIZE];
    while (1)
    {
        buffer[0] = '\0';
        int n = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Client %d (in transaction): %s\n", session_id, buffer);
            if (strcmp(buffer, "COMMIT TRANSACTION") == 0) {
                break;
            } else {
                // Here you would handle the database commands like GET, SET, ADD, DELETE
                // For simplicity, we just echo back the command received
                char response[BUFFER_SIZE + 19]; // 19 is the length of "Command executed: "
                snprintf(response, sizeof(response), "Command executed: %s", buffer);
                send(client_socket, response, strlen(response), 0);
            }
        } else {
            printf("Client %d disconnected during transaction.\n", session_id);
            break;
        }
    }
}

// Proccess the connection with each client
void process_connection(int client_socket, int session_id, int db_fd) {
    
    printf("Connection accepted. PID: %d, Session ID: %d\n", getpid(), session_id);
    // Send welcome message
    send(client_socket, SERVER_MESSAGE, strlen(SERVER_MESSAGE), 0);
    char buffer[BUFFER_SIZE];

    while (1)
    {
        buffer[0] = '\0';
        int n = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Client %d: %s\n", session_id, buffer);
            if (strcmp(buffer, "BEGIN TRANSACTION") == 0) {
                // Wait for exclusive access to the database
                sem_wait(&db_sem);
                send(client_socket, "Transaction started. You have exclusive access to the database.\n", 64, 0);
                database_transaction(client_socket, session_id, db_fd);
                send(client_socket, "Transaction committed. You no longer have exclusive access to the database.\n", 70, 0);
                sem_post(&db_sem);
            } else if (strcmp(buffer, "exit") == 0) {
                printf("Client %d disconnected.\n", session_id);
                break;
            } else {
                send(client_socket, "Unknown command. Please use BEGIN TRANSACTION to use the database.\n", 68, 0);
                continue;
            }
        } else {
            printf("Client %d disconnected.\n", session_id);
            break;
        }
    }
    close(client_socket);
}

void main(int argc, char *argv[]) {
    int server_fd, client_fd;
    int sin_size, port, session_counter = 0;
    struct sockaddr_in client_socket;
    char *host;
    char *ip;

    if (argc != 3) {
        printf("Usage: %s <ip-address> <port>\n", argv[0]);
        exit(EXIT_SUCCESS);
    } else {
        port = atoi(argv[2]);
        if (port <= 1024 || port >= 65535) {
            printf("Please provide a valid port number between 1025 and 65534.\n");
            exit(EXIT_FAILURE);
        }
        if (strlen(argv[1]) > MAXHOSTNAME) {
            printf("Hostname too long. Max length is %d characters.\n", MAXHOSTNAME);
            exit(EXIT_FAILURE);
        }
        ip = argv[1];
    }

    if ((server_fd = establish(ip, port)) < 0) {
            perror("Couldn't establish the server socket!");
            exit(EXIT_FAILURE);
    }

    sem_init(&connection_sem, 1, CONN_LIMIT);
    sem_init(&db_sem, 1, 1);

    int database_fd = open(DB_PATH, O_RDWR);
    if (database_fd < 0) {
        perror("Couldn't open the database file!");
        close(server_fd);
        close(database_fd);
        exit(EXIT_FAILURE);
    }


    signal(SIGCHLD, zombie_catcher);
    printf("Server is up!\n"
            "PID: %d\n"
            "Listening for connections...\n", getpid());
    for (;;) {
        sin_size = sizeof(struct sockaddr_in);
        
        if ((client_fd = accept(server_fd, (struct sockaddr *) &client_socket, &sin_size)) < 0) {
            if (errno == EINTR) {
                close(server_fd);
                perror("Error getting connection.");
                exit(EXIT_FAILURE);
            } else {
                continue;
            }
        }
        printf("Connection received from %s\n", inet_ntoa(client_socket.sin_addr));
        switch (fork())
        {
        case -1:    // Fallo el fork, exploto todo.
            perror("Forking error!");
            close(client_fd);
            close(server_fd);
            exit(EXIT_FAILURE);

        case 0:     // El hijo procesa la conexion en la funcion correspondiente.
            printf("Child process created with PID: %d\n", getpid());
            close(server_fd);
            sem_wait(&connection_sem);
            process_connection(client_fd, session_counter, database_fd);
            sem_post(&connection_sem);
            exit(EXIT_SUCCESS);
        
        default:    // el padre sigue y vuelve a esperar mas conexiones
            session_counter ++;
            close(client_fd);
            continue;
        }
    }
}