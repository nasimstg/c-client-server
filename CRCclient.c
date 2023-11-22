#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <inttypes.h>

#define PORT 8080
#define BUFFER_SIZE 2048

char login_name[100];

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

void *receive_handler(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_received <= 0) {
            printf("Disconnected from server.\n");
            close(sock);
            exit(0);
        }
        printf("\n%s\n", buffer);
        printf("[%s]: ", login_name); // Prompt for the next message
        fflush(stdout); // Flush the stdout buffer to immediately display the prompt
    }
}

int main() {
    struct sockaddr_in server_addr;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to the server\n");
    printf("Enter your login name: ");
    fgets(login_name, sizeof(login_name), stdin);
    login_name[strcspn(login_name, "\n")] = 0; // Remove the newline character

    send(client_socket, login_name, strlen(login_name), 0);

    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_handler, &client_socket);

    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    char recipient[100];

    // Initialize the CRC-32 table
    generate_crc32_table();

    while (1) {
        printf("[%s]: ", login_name);
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0; // Remove the newline character

        printf("Enter recipient login name: ");
        fgets(recipient, sizeof(recipient), stdin);
        recipient[strcspn(recipient, "\n")] = 0; // Remove the newline character

        uint32_t crc_value = compute_crc32(message);
        snprintf(buffer, sizeof(buffer), "<MSG><FROM>%s</FROM><TO>%s</TO><BODY>%s</BODY><CRC>%" PRIu32 "</CRC></MSG>", login_name, recipient, message, crc_value);
        send(client_socket, buffer, strlen(buffer), 0);
    }

    close(client_socket);
    return 0;
}