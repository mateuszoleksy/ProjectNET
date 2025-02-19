#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/tcp.h>         /* for TCP_MAXSEG */
#include <sys/wait.h>
#include <stdbool.h>
#include <pwd.h>
#include <grp.h>

#define BUFFER_SIZE 10240
#define PORT 8900

int id = 0;

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

int handle_client(int client_sock, char folder[256]) {
    char buffer[BUFFER_SIZE];
    char filename[256];
    char src[256];
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);

        if (bytes_received > 0)
            printf("Received command: %s\n", buffer);
        if (bytes_received <= 0)
            return 0;

        if (strncmp(buffer, "UPLOAD", 6) == 0) {
            int filesize;
            sscanf(buffer + 7, "%s %d", filename, &filesize);

            sprintf(src, "%s/%s", folder, filename);
            fprintf(stderr, "%s %s", folder, filename);
            FILE *file = fopen(src, "wb");
            if (!file) {
                perror("File open error");
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
            sscanf(buffer + 9, "%s", filename);

            sprintf(src, "%s/%s", folder, filename);
            FILE *file = fopen(src, "rb");
            if (!file) {
                send(client_sock, "ERROR File not found", strlen("ERROR File not found"), 0);
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
            sprintf(src, "%s %s", "ls", folder);
            FILE *fp = popen(src, "r");
            if (!fp) {
                send(client_sock, "ERROR Listing files", strlen("ERROR Listing files"), 0);
            }

            if (fgets(buffer, sizeof(buffer), fp) <= 0)
            {
                send(client_sock, "Nothing to see!", strlen("Nothing to see!"), 0);
                close(fp);
                return 0;
            } else {
                 send(client_sock, buffer, strlen(buffer), 0);
            }

            while (fgets(buffer, sizeof(buffer), fp)) {
                send(client_sock, buffer, strlen(buffer), 0);
            }

            pclose(fp);
        } else if (strncmp(buffer, "DELETE", 6) == 0) {
            sscanf(buffer + 7, "%s", filename);
            sprintf(src, "/%s/%s", folder, filename);

            if (remove(src) == 0) {
                send(client_sock, "DELETE OK", strlen("DELETE OK"), 0);
            } else {
                send(client_sock, "ERROR File not found", strlen("ERROR File not found"), 0);
            }
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            send(client_sock, "QUIT OK", strlen("QUIT OK"), 0);
            printf("Client disconnected.\n");
            close(client_sock);
            return -1;
        } else {
            send(client_sock, "ERROR Unknown command", strlen("ERROR Unknown command"), 0);
        }

        return 0;
    };

bool find_client(char login[256], char password[256]) {
    char buffer[256];
    char read_pass[256];
    char read_login[256];
    
            FILE *fp = fopen("conf/configuration", "rb");
            if (!fp) {
                perror("Cannot open file");
                return false;
            }

            while (fgets(buffer, sizeof(buffer), fp)) {
                sscanf(buffer, "%s %s", read_login, read_pass);
                if (strncmp(read_login, login, strlen(login)) == 0)
                {
                    if (strncmp(read_pass, password, strlen(password)) == 0)
                    {
                        pclose(fp);
                        return true;
                    }
                }
            }

            pclose(fp);
            return false;
}

int main(int argc, char *argv[]) {
    pid_t childpid;
    char str[256];
    char folder[256];

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

    int listenfd;

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

    bool loggon[1024] = {false};

    if (strcmp(argv[1], "tcp") == 0) {
        if (setsockopt(server_sock, SOCK_STREAM, SO_RCVTIMEO, &time, sizeof(time)) < 0)
        {
            perror("Setting delay on socket failed.");
        }
    } else if (strcmp(argv[1], "sctp") == 0) {
        if (setsockopt(server_sock, SOCK_SEQPACKET, SO_RCVTIMEO, &time, sizeof(time)) < 0)
        {
            perror("Setting delay on socket failed.");
        }
    }

    signal(SIGCHLD, sig_chld);
    signal(SIGPIPE, sig_handler);
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int stan = 0;

     printf("Server is running on port %d using %s, listening...\n", PORT, argv[1]);

    while (1)
    {

        struct stat st = {0};

        int client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_len);
        if (client_sock < 0) {
                continue;
        }
        
        inet_ntop(AF_INET, (struct sockaddr  *) &client_addr.sin_addr,  str, sizeof(str));
        fprintf(stderr, "Client connected. IP address: %s \n", str);

        if ( (childpid = fork()) == 0) {
            char password[256];
            char login[256];
                
            id = id + 1;
            for (;;) {
            
                if (loggon[id]) {
                    if (stat(login, &st) == -1) {
                     
                        mkdir(login, 0755);
                     
                    }

                    int er = handle_client(client_sock, login);
                    if (er == 1)
                        exit(0);

                } else {
                    int bytes = recv(client_sock, buffer, BUFFER_SIZE, 0);
                    if (bytes <= 0)
                        continue;
                    if (strncmp(buffer, "LOGIN", 5) == 0)
                    {
                       stan = 1;
                    } else if (strncmp(buffer, "REGISTER", 8) == 0) {
                        stan = 2;
                    } else if (stan == 1)
                    {
                        sleep(1);
                        sscanf(buffer, "%s %s", login, password);
                        loggon[id] = find_client(login, password);
                        if (loggon[id])
                        {
                            send(client_sock, "LOGIN OK", strlen("LOGIN OK"), 0);
                            sleep(1);
                        } else {
                            send(client_sock, "ERROR, login in failed. \n", strlen("ERROR, login in failed.\n"), 0);
                        }
                       stan = 0;
                    } else if (stan == 2)
                    {
                        sleep(1);
                        sscanf(buffer, "%s %s", login, password);
                        loggon[id] = find_client(login, password);
                        if (!loggon[id])
                        {
                            FILE *fp = fopen("conf/configuration", "a");
                            if (!fp) {
                                send(client_sock, "ERROR register, please try again. \n", strlen("ERROR register, please try again. \n"), 0);
                                loggon[id] = false;
                                perror("File");
                                continue;
                            }
                            fprintf(fp, buffer);
                            pclose(fp);
                            send(client_sock, "REGISTER OK \n", strlen("REGISTER OK \n"), 0);
                            sleep(1);
                            loggon[id] = true;
                            stan = 0;
                        } else
                        {
                            send(client_sock, "ERROR, register failed. \n", strlen("ERROR, register failed. \n"), 0);
                        }

                    } else
                    {
                        send(client_sock, "ERROR, unknown command. \n", strlen("ERROR, unknown command. \n"), 0);
                    }
                }
        }

        fflush(stdout);
        }
    }
    close(server_sock);
    return 0;
}
