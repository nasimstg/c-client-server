#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 100

char *clients[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int register_client(const char *client_name, int sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = strdup(client_name);
            client_sockets[i] = sock;
            pthread_mutex_unlock(&clients_mutex);
            return 1; // Successfully registered
        } else if (strcmp(clients[i], client_name) == 0) {
            pthread_mutex_unlock(&clients_mutex);
            return 0; // Client name already exists
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1; // Max clients reached
}

int find_client(const char *client_name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && strcmp(clients[i], client_name) == 0) {
            return client_sockets[i]; // Return the socket descriptor of the client
        }
    }
    return -1; // Client not found
}

void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sock) {
            free(clients[i]);
            clients[i] = NULL;
            client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_handler(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[1024] = {0};
    read(sock, buffer, 1024);

    printf("Received login name from client: %s\n", buffer);

    int registration_status = register_client(buffer, sock);
    if (registration_status == 1) {
        send(sock, "Registration successful", 23, 0);
    } else if (registration_status == 0) {
        send(sock, "Login name already exists", 26, 0);
    } else {
        send(sock, "Server full", 11, 0);
    }

    char sender[1024], recipient[1024], message[1024];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            printf("Client disconnected or error occurred.\n");
            remove_client(sock);
            break;
        }
        if (sscanf(buffer, "<MSG><FROM>%[^<]</FROM><TO>%[^<]</TO><BODY>%[^<]</BODY></MSG>", sender, recipient, message) == 3) {
            int recipient_socket = find_client(recipient);
            if (recipient_socket != -1) {
                char forward_message[2048];
                snprintf(forward_message, sizeof(forward_message), "%s: %s", sender, message);
                send(recipient_socket, forward_message, strlen(forward_message), 0);
            } else {
                send(sock, "Recipient not found or offline", 30, 0);
            }
        }
    }

    close(sock);
    free(client_socket);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is ready to accept connections on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Client connected from IP: %s, Port: %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        pthread_t tid;
        int *pclient = malloc(sizeof(int));
        *pclient = new_socket;
        pthread_create(&tid, NULL, client_handler, pclient);
    }

    return 0;
}