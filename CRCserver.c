#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <inttypes.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_CLIENTS 100

char *clients[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t crc32_table[256];

void generate_crc32_table() {
    uint32_t polynomial = 0x04C11DB7;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i << 24;
        for (uint32_t j = 0; j < 8; j++) {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ polynomial;
            else
                crc <<= 1;
        }
        crc32_table[i] = crc;
    }
}

uint32_t compute_crc32(const char *data) {
    uint32_t crc = 0xFFFFFFFF;
    while (*data) {
        uint8_t byte = *data++;
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ byte) & 255];
    }
    return crc;
}

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
    char buffer[BUFFER_SIZE];
    char sender[1024], recipient[1024], message[1024];
    uint32_t received_crc, computed_crc;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_received <= 0) {
            printf("Client disconnected or error occurred.\n");
            remove_client(sock);
            break;
        }

        if (sscanf(buffer, "<MSG><FROM>%[^<]</FROM><TO>%[^<]</TO><BODY>%[^<]</BODY><CRC>%" PRIu32 "</CRC></MSG>", sender, recipient, message, &received_crc) == 4) {
            computed_crc = compute_crc32(message);
            if (computed_crc != received_crc) {
                printf("CRC error detected for message from %s to %s.\n", sender, recipient);
                send(sock, "CRC error detected.", 19, 0);
                continue;
            }

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

    // Initialize the CRC-32 table
    generate_crc32_table();

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