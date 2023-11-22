#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char login_name[1024];
    char server_response[1024];

    // Creating socket file descriptor
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server\n");

    // Prompt user for login name
    printf("Enter your login name: ");
    fgets(login_name, sizeof(login_name), stdin);
    login_name[strcspn(login_name, "\n")] = 0;

    // Send login name to the server
    send(client_socket, login_name, strlen(login_name), 0);

    // Wait for server's response and display it
    read(client_socket, server_response, sizeof(server_response));
    printf("Server response: %s\n", server_response);

    while (1) {
        printf("Enter recipient's login name: ");
        char recipient[1024];
        fgets(recipient, sizeof(recipient), stdin);
        recipient[strcspn(recipient, "\n")] = 0;

        printf("Enter your message: ");
        char message[1024];
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        char full_message[2048];
        snprintf(full_message, sizeof(full_message), "<MSG><FROM>%s</FROM><TO>%s</TO><BODY>%s</BODY></MSG>", login_name, recipient, message);
        send(client_socket, full_message, strlen(full_message), 0);

        memset(server_response, 0, sizeof(server_response));
        read(client_socket, server_response, sizeof(server_response));
        printf("Server response: %s\n", server_response);
    }

    return 0;
}