// General includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Sockets includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

// Defines and constants
#define PORT 13000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

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

int main(int argc, char* argv[]) {
    int server_fd;
    char *ip;
    int port;
    struct hostent *server;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    if(argc == 3) {
        ip = argv[1];
        port = atoi(argv[2]);
    } else {
        ip = SERVER_IP;
        port = PORT;
        printf("Usage: %s <ip-address> <port>\nNo port or ip provided, using defaults %s/%d\n", argv[0], SERVER_IP, PORT);
    }

    if((server=gethostbyname(ip)) == NULL) {
        printf("ERROR, no such host\n");
        exit(EXIT_FAILURE);
    }


    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) < 0) {
        perror("Invalid address/ Address not supported");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server on port %d\n", port);

    // Communication with server
    pthread_t conn_thread;
    if(pthread_create(&conn_thread, NULL, (void *)check_connection, (void *)&server_fd) != 0) {
        perror("Failed to create connection check thread");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // Wait for welcome message
    
    int msg_size = recv(server_fd, buffer, BUFFER_SIZE - 1, 0);
    buffer[msg_size] = '\0';
    printf("Server@%s: %s\n", ip, buffer);
    
    while (1) {
        // Get user input
        printf("Enter message (type 'exit' to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        
        // Send message to server
        send(server_fd, buffer, strlen(buffer), 0);
        
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }
        // Receive response from server
        msg_size = recv(server_fd, buffer, BUFFER_SIZE - 1, 0);
        if (msg_size > 0) {
            buffer[msg_size] = '\0';
            printf("Server@%s: %s\n", ip, buffer);
        } else {
            printf("No response from server or connection closed.\n");
            break;
        }
    }

    pthread_cancel(conn_thread);
    pthread_join(conn_thread, NULL);
    close(server_fd);
    return 0;
}