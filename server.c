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
#include <netinet/in.h>
#include <netinet/tcp.h>         /* for TCP_MAXSEG */
#include <sys/wait.h>
#include <stdbool.h>
#include <pwd.h>
#include <grp.h>

#define BUFFER_SIZE 111500
#define PORT 8900
char source[] = "/home/matt2/Downloads/";

int id = 0;

void
sig_chld(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0);
}

void
sig_handler(int signo) {
    pid_t pid;
    int stat;
}

int handle_client(int client_sock, char folder[256]) {
    char buffer[BUFFER_SIZE];
    char filename[256];
    char src[512];
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);

    if (bytes_received > 0)
        printf("Received command: %s from %s \n", buffer, folder);
    if (bytes_received <= 0)
        return 1;

    if (strncmp(buffer, "UPLOAD", 6) == 0) {
        int filesize;
        sscanf(buffer + 7, "%s %d", filename, &filesize);

        sprintf(src, "%s%s/%s", source, folder, filename);
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
        return 0;
    } else if (strncmp(buffer, "DOWNLOAD", 8) == 0) {
        sscanf(buffer + 9, "%s", filename);

        sprintf(src, "%s%s/%s", source, folder, filename);
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

        pclose(file);
        return 0;
    } else if (strncmp(buffer, "LIST", 4) == 0) {
        char buffer_send[BUFFER_SIZE];
        sprintf(src, "ls %s%s", source, folder);
        FILE *fp = popen(src, "r");
        if (!fp) {
            send(client_sock, "ERROR Listing files", strlen("ERROR Listing files"), 0);
        }
        if (fgets(buffer, sizeof(buffer), fp) <= 0) {
            send(client_sock, "Nothing to see!", strlen("Nothing to see!"), 0);
            pclose(fp);
            return 0;
        }
        memset(buffer_send, 0,BUFFER_SIZE);
        strcat(buffer_send, buffer);
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            strcat(buffer_send, buffer);
        }
        send(client_sock, buffer_send, strlen(buffer_send), 0);

        pclose(fp);
        return 0;
    } else if (strncmp(buffer, "DELETE", 6) == 0) {
        sscanf(buffer + 7, "%s", filename);
        sprintf(src, "%s%s/%s", source, folder, filename);

        if (remove(src) == 0) {
            send(client_sock, "DELETE OK", strlen("DELETE OK"), 0);
        } else {
            send(client_sock, "ERROR File not found", strlen("ERROR File not found"), 0);
        }
        return 0;
    } else if (strncmp(buffer, "QUIT", 4) == 0) {
        send(client_sock, "QUIT OK", strlen("QUIT OK"), 0);
        return 1;
    } else if (strncmp(buffer, "CAT", 3) == 0) {
        char buffer_send[BUFFER_SIZE];
        sscanf(buffer + 4, "%s", filename);
        sprintf(src, "cat %s%s/%s", source, folder, filename);
        FILE *fp = popen(src, "r");
        if (!fp) {
            send(client_sock, "ERROR displaying files", strlen("ERROR displaying files"), 0);
        }

        memset(buffer_send, 0, BUFFER_SIZE);
        if (fgets(buffer, sizeof(buffer), fp) <= 0) {
            send(client_sock, "Nothing to see!", strlen("Nothing to see!"), 0);
            pclose(fp);
            return 0;
        }
        strcat(buffer_send, buffer);


        while (fgets(buffer, sizeof(buffer), fp)) {
            strcat(buffer_send, buffer);
        }
        send(client_sock, buffer_send, strlen(buffer_send), 0);

        pclose(fp);
        return 0;
    } else {
        send(client_sock, "ERROR Unknown command!\n", strlen("ERROR Unknown command!\n"), 0);
        return 0;
    }

    return 1;
};

bool find_client(char login[256], char password[256]) {
    char buffer[256];
    char read_pass[256];
    char read_login[256];
    char conf[256];
    sprintf(conf, "%sconf/configuration", source);
    FILE *fp = fopen(conf, "rb");
    if (!fp) {
        perror("Cannot open file");
        return false;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        sscanf(buffer, "%s %s", read_login, read_pass);
        if (strcmp(login, read_login) == 0) {
            if (strcmp(read_pass, password) == 0) {
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
    char folder[256];
    char config[256];

    if (argc != 2) {
        printf("Usage: %s [tcp|sctp]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int server_sock;
    if (strcmp(argv[1], "tcp") == 0) {
        server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    } else if (strcmp(argv[1], "sctp") == 0) {
        server_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
    } else {
        printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
        return EXIT_FAILURE;
    }

    int option = 0; // 0 = obsługuje IPv4 i IPv6
    if (setsockopt(server_sock, IPPROTO_IPV6, IPV6_V6ONLY, &option, sizeof(option)) < 0) {
        perror("Błąd ustawienia opcji IPV6_V6ONLY");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (server_sock < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int listenfd;

    struct sockaddr_in6 server_addr;
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(PORT);
    server_addr.sin6_addr = in6addr_any;

    if (bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return EXIT_FAILURE;
    }

    if (listenfd = listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        return EXIT_FAILURE;
    }

    struct timeval time;
    time.tv_usec = 0;
    time.tv_sec = 60;

    bool loggon[1024] = {false};

    if (strcmp(argv[1], "tcp") == 0) {
        if (setsockopt(server_sock, SOCK_STREAM, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
            perror("Setting delay on socket failed.");
        }
    } else if (strcmp(argv[1], "sctp") == 0) {
        if (setsockopt(server_sock, SOCK_STREAM, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
            perror("Setting delay on socket failed.");
        }
    }
    signal(SIGPIPE, sig_handler);
    signal(SIGCHLD, sig_chld);

    struct sockaddr_in6 client_addr6;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int stan = 0;
    sprintf(config, "%sconf/login", source);
    FILE *fp = fopen(config, "w");
    if (!fp) {
        perror("File: ");
        exit(1);
    }
    fclose(fp);


    printf("Server is running on port %d using %s, listening...\n", PORT, argv[1]);

    while (1) {
        struct stat st = {0};

        int client_sock = accept(server_sock, (struct sockaddr *) &client_addr6, &client_len);
        if (client_sock < 0) {
            continue;
        }

        if ((childpid = fork()) == 0) {
            char str[256];
            char conf[256];
            if (inet_ntop(AF_INET6, (struct sockaddr *) &client_addr6.sin6_addr, str, sizeof(str)) > 0)
                fprintf(stderr, "Client connected. IP address: %s \n", str);
            char password[256];
            char login[256];
            id = id + 1;
            for (;;) {
                if (loggon[id]) {
                    sprintf(password, "%s%s", source, login);
                    if (stat(password, &st) == -1) {
                        mkdir(password, 0755);
                    }

                    int er = handle_client(client_sock, login);
                    if (er == 1) {
                        sprintf(conf, "%sconf/login", source);
                        FILE *fp = fopen(conf, "rb");
                        sprintf(conf, "%sconf/temp", source);
                        FILE *fhelp = fopen(conf, "w");
                        if (!fp && !fhelp) {
                            perror("file");
                        } else {
                            char read_login[256];
                            char read_ip[256];
                            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                                sscanf(buffer, "%s %s", read_login, read_ip);
                                if (strcmp(read_login, login) != 0) {
                                    fprintf(fhelp, buffer);
                                }
                            }
                            pclose(fhelp);
                            pclose(fp);
                            sprintf(conf, "%sconf/login", source);
                            remove(conf);
                            char confTwo[256];
                            sprintf(confTwo, "%sconf/temp", source);
                            sprintf(conf, "%sconf/login", source);
                            rename(confTwo, conf);
                            printf("Client disconnected. IP: %s.\n", str);
                            close(client_sock);
                            id = id - 1;
                            loggon[id] = 0;
                            exit(0);
                        }
                    }
                } else {
                    int bytes = recv(client_sock, buffer, BUFFER_SIZE, 0);
                    if (bytes <= 0) {
                        fprintf(stderr, "Client disconnected. IP: %s.\n", str);
                        id = id - 1;
                        loggon[id] = 0;
                        exit(0);
                    }
                    if (strncmp(buffer, "LOGIN", 5) == 0) {
                        stan = 1;
                    } else if (strncmp(buffer, "REGISTER", 8) == 0) {
                        stan = 2;
                    } else if (stan == 1) {
                        char read_login[256];
                        char read_ip[256];
                        sscanf(buffer, "%s %s", login, password);
                        fprintf(stderr, buffer);
                        loggon[id] = find_client(login, password);
                        sprintf(conf, "%sconf/login", source);
                        FILE *fp = fopen(conf, "rb");
                        if (!fp) {
                            perror("File");
                        } else {
                            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                                sscanf(buffer, "%s, %s", read_login, read_ip);
                                if (strncmp(read_login, login, strlen(login)) == 0) {
                                    loggon[id] = false;
                                    break;
                                }
                            }
                        }
                        pclose(fp);
                        if (loggon[id]) {
                            send(client_sock, "LOGIN OK", strlen("LOGIN OK"), 0);
                            fprintf(stderr, "Client connected with login: %s and IP %s \n", login, str);
                            sprintf(conf, "%sconf/login", source);
                            FILE *file = fopen(conf, "a");
                            if (!file) {
                                perror("file");
                            } else {
                                fprintf(file, "%s %s\n", login, str);
                            }

                            pclose(file);
                        } else {
                            send(client_sock, "ERROR, login failed. \n", strlen("ERROR, login failed.\n"), 0);
                        }
                        stan = 0;
                    } else if (stan == 2) {
                        sscanf(buffer, "%s %s", login, password);
                        fprintf(stderr, buffer);
                        loggon[id] = find_client(login, password);
                        if (!loggon[id]) {
                            sprintf(conf, "%sconf/configuration", source);
                            FILE *fp = fopen(conf, "a");
                            if (!fp) {
                                send(client_sock, "ERROR register, please try again. \n",
                                     strlen("ERROR register, please try again. \n"), 0);
                                loggon[id] = false;
                                perror("File");
                                continue;
                            } else {
                                fprintf(fp, buffer);
                            }
                            pclose(fp);
                            send(client_sock, "REGISTER OK \n", strlen("REGISTER OK \n"), 0);
                            sprintf(conf, "%sconf/login", source);
                            FILE *file = fopen(login, "a");
                            if (!file) {
                                perror("file");
                            } else {
                                fprintf(file, "%s %s\n", login, str);
                            }
                            pclose(file);
                            fprintf(stderr, "Client connected with login: %s and IP %s \n", login, str);
                            loggon[id] = true;
                            stan = 0;
                        } else {
                            send(client_sock, "ERROR, register failed. \n", strlen("ERROR, register failed. \n"), 0);
                            loggon[id] = false;
                        }
                    } else {
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
