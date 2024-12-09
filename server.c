#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024
#define PORT 9000

void
sig_chld(int signo)
{
    pid_t	pid;
    int	stat;
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0);
}

void
sig_handler(int signo)
{
    pid_t  pid;
    int stat;
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        printf("Received command: %s\n", buffer);

        if (strncmp(buffer, "UPLOAD", 6) == 0) {
            char filename[256];
            int filesize;
            sscanf(buffer + 7, "%s %d", filename, &filesize);

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

            fclose(file);
            send(client_sock, "UPLOAD OK", strlen("UPLOAD OK"), 0);
        } else if (strncmp(buffer, "DOWNLOAD", 8) == 0) {
            char filename[256];
            sscanf(buffer + 9, "%s", filename);

            FILE *file = fopen(filename, "rb");
            if (!file) {
                send(client_sock, "ERROR File not found", strlen("ERROR File not found"), 0);
                continue;
            }

            fseek(file, 0, SEEK_END);
            int filesize = ftell(file);
            rewind(file);

            sprintf(buffer, "%s %s %d", "READ", filename, filesize);
            send(client_sock, buffer, strlen(buffer), 0);
            sleep(1);
            memset(buffer, 0, BUFFER_SIZE);
            while ((bytes_received = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
                send(client_sock, buffer, bytes_received, 0);
            }

            fclose(file);
        } else if (strncmp(buffer, "LIST", 4) == 0) {
            FILE *fp = popen("ls", "r");
            if (!fp) {
                send(client_sock, "ERROR Listing files", strlen("ERROR Listing files"), 0);
                continue;
            }

            while (fgets(buffer, sizeof(buffer), fp)) {
                send(client_sock, buffer, strlen(buffer), 0);
            }

            pclose(fp);
        } else if (strncmp(buffer, "DELETE", 6) == 0) {
            char filename[256];
            sscanf(buffer + 7, "%s", filename);

            if (remove(filename) == 0) {
                send(client_sock, "DELETE OK", strlen("DELETE OK"), 0);
            } else {
                send(client_sock, "ERROR File not found", strlen("ERROR File not found"), 0);
            }
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            send(client_sock, "QUIT OK", strlen("QUIT OK"), 0);
            break;
        } else {
            send(client_sock, "ERROR Unknown command", strlen("ERROR Unknown command"), 0);
        }
    }

    close(client_sock);
}

int main(int argc, char *argv[]) {
    pid_t childpid;

    if (argc != 2) {
        printf("Usage: %s [tcp|sctp]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_sock;
    if (strcmp(argv[1], "tcp") == 0) {
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(argv[1], "sctp") == 0) {
        server_sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
    } else {
        printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
        return EXIT_FAILURE;
    }

    if (server_sock < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return EXIT_FAILURE;
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        return EXIT_FAILURE;
    }

    struct timeval time;
    time.tv_usec = 0;
    time.tv_sec = 5;

    if (strcmp(argv[1], "tcp") == 0) {
        if (setsockopt(server_sock, SOCK_STREAM, SO_RCVTIMEO, &delay, sizeof(delay)) < 0)
        {
            perror("Setting delay on socket failed.");
        }
    } else if (strcmp(argv[1], "sctp") == 0) {
        if (setsockopt(server_sock, SOCK_SEQPACKET, SO_RCVTIMEO, &delay, sizeof(delay)) < 0)
        {
            perror("Setting delay on socket failed.");
        }
    }

    signal(SIGCHLD, sig_chld);
    signal(SIGPIPE, sig_handler);

    printf("Server is running on port %d using %s...\n", PORT, argv[1]);
    if ( (childpid = fork()) == 0) {
        while (1) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_len);
            if (client_sock < 0) {
                perror("Accept failed");
                continue;
            }

            printf("Client connected.\n");
            handle_client(client_sock);
        }
    }

    close(server_sock);
    return 0;
}
