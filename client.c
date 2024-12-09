#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>

#define BUFFER_SIZE 1024
#define PORT 9000

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s [tcp|sctp] <server_ip>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int client_sock;
    if (strcmp(argv[1], "tcp") == 0) {
        client_sock = socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(argv[1], "sctp") == 0) {
        client_sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
    } else {
        printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
        return EXIT_FAILURE;
    }

    if (client_sock < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_sock);
        return EXIT_FAILURE;
    }

    printf("Connected to server.\n");

    char buffer_send[BUFFER_SIZE];
     char buffer[BUFFER_SIZE];
    while (1) {
        fprintf(stderr, "Enter command: ");
        fgets(buffer_send, BUFFER_SIZE, stdin);
        buffer_send[strcspn(buffer_send, "\n")] = '\0';  // Remove newline

        if (strncmp(buffer_send, "UPLOAD", 6) == 0) {
            char filename[256];
            sscanf(buffer_send + 7, "%s", filename);

            FILE *file = fopen(filename, "rb");
            if (!file) {
                perror("File not found");
                continue;
            }

            fseek(file, 0, SEEK_END);
            int filesize = ftell(file);
            rewind(file);

            memset(buffer_send, 0, BUFFER_SIZE);
            sprintf(buffer_send, "%s %s %d ", "UPLOAD", filename, filesize);
            send(client_sock, buffer_send, strlen(buffer_send), 0);

            int bytes_received = 0;
            while ((bytes_received = fread(buffer_send, sizeof(char), BUFFER_SIZE, file)) > 0) {
                send(client_sock, buffer_send, bytes_received, 0);
            }

            fclose(file);
        } else if (strncmp(buffer_send, "LS", 6) == 0)
        {
            FILE *fp = popen("ls", "r");
            if (!fp) {
                perror("ERROR Listing files");
                continue;
            }

            memset(buffer, 0, BUFFER_SIZE);
            while (fgets(buffer, sizeof(buffer), fp)) {
                fprintf(stderr, "%s", buffer);
            }

            sprintf(buffer_send, "%s", "CONTINUE");
            send(client_sock, buffer_send, strlen(buffer_send), 0);

            fclose(fp);
        }
        else
        {
            send(client_sock, buffer_send, strlen(buffer_send), 0);
        }
        sleep(1);

        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (strncmp(buffer_send, "QUIT", 4) == 0) {
            printf("Disconnecting...\n");
            break;
        }   else if (strncmp(buffer, "READ", 4) == 0)
        {
            if (bytes_received > 0) {
            char filename[256];
                int filesize;
                sscanf(buffer + 5, "%s %d", filename, &filesize);

                FILE *file = fopen(filename, "wb");
                if (!file) {
                    perror("File open error");
                    break;
                }

                memset(buffer, 0, BUFFER_SIZE);
                int received = 0;
                while (received < filesize) {
                    int bytes = recv(client_sock, buffer, BUFFER_SIZE, 0);
                    fwrite(buffer, sizeof(char), bytes, file);
                    received += bytes;
                }

                memset(buffer, 0, BUFFER_SIZE);
                fclose(file);
            }
        } else {
            if (bytes_received > 0) {
                fprintf(stderr, "Server response: \n%s\n", buffer);
            }
        }
    }

    close(client_sock);
    return 0;
}
