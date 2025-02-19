#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#define BUFFER_SIZE 111500
#define PORT 8900

void sig_handler(int signum) {
    fprintf(stderr, "Something went wrong! \n");
    exit(1);
}

int main(int argc, char *argv[]) {
    bool loggon = false;

    if (argc <= 2) {
        printf("Usage: %s [tcp|sctp] <server_ip> [optional]server_name\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct in6_addr *inetaddrp[2];
    char **pptr;
    struct in6_addr inetaddr;
    struct hostent *hp;
    struct hostent m_hp;
    struct servent *sp;
    char str6[INET6_ADDRSTRLEN];
    struct sockaddr_in server_addr;
    struct sockaddr_in6 server_addr6;
    int client_sock;
    if (argc == 3) {
        if (inet_pton(AF_INET, argv[2], &server_addr.sin_addr) > 0) {
            if (strcmp(argv[1], "tcp") == 0) {
                client_sock = socket(AF_INET, SOCK_STREAM, 0);
            } else if (strcmp(argv[1], "sctp") == 0) {
                client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
            } else {
                printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
                return EXIT_FAILURE;
            }
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);

            if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0) {
                perror("Connection failed");
                close(client_sock);
                return EXIT_FAILURE;
            }
        } else if (inet_pton(AF_INET6, argv[2], &server_addr6.sin6_addr) > 0) {
            if (strcmp(argv[1], "tcp") == 0) {
                client_sock = socket(AF_INET6, SOCK_STREAM, 0);
            } else if (strcmp(argv[1], "sctp") == 0) {
                client_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
            } else {
                printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
                return EXIT_FAILURE;
            }
            server_addr6.sin6_family = AF_INET6;
            server_addr6.sin6_port = htons(PORT);

            if (connect(client_sock, (struct sockaddr *) &server_addr6, sizeof (server_addr6)) < 0) {
                perror("Connection failed");
                close(client_sock);
                return EXIT_FAILURE;
            }
        }
    } else if (argc == 4) {
        if ((hp = gethostbyname(argv[3])) == NULL) {
            if (inet_pton(AF_INET6, argv[3], &inetaddr) <= 0) {
                printf("gethostbyname error for host: %s: %s\n",
                       argv[3], hstrerror(h_errno));
                return 1;
            }
            inetaddrp[0] = &inetaddr;
            inetaddrp[1] = NULL;
            m_hp.h_addr_list = (char **) inetaddrp;
            m_hp.h_addrtype = AF_INET6;
            m_hp.h_name = argv[3];
            hp = &m_hp;
        }

        printf("Official hostname: %s\n", hp->h_name);

        switch (hp->h_addrtype) {
            case AF_INET:
                pptr = hp->h_addr_list;

                if (strcmp(argv[1], "tcp") == 0) {
                    client_sock = socket(AF_INET, SOCK_STREAM, 0);
                } else if (strcmp(argv[1], "sctp") == 0) {
                    client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
                } else {
                    printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
                    return EXIT_FAILURE;
                }

                for (; *pptr != NULL; pptr++) {
                    bzero(&server_addr, sizeof (server_addr));
                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(PORT);
                    memcpy(&server_addr.sin_addr, *pptr, sizeof(struct in_addr));

                    inet_ntop(AF_INET, (struct sockaddr *) *pptr, str6, sizeof (str6));
                    printf("\ttrying: %s\n", str6);

                    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof (server_addr)) == 0)
                        break; /* success */
                    fprintf(stderr, "IPV4 connect error : %s \n", strerror(errno));
                    close(client_sock);
                }
                break;

            case AF_INET6:
                pptr = hp->h_addr_list;

                if (strcmp(argv[1], "tcp") == 0) {
                    client_sock = socket(AF_INET6, SOCK_STREAM, 0);
                } else if (strcmp(argv[1], "sctp") == 0) {
                    client_sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
                } else {
                    printf("Unsupported protocol. Use 'tcp' or 'sctp'.\n");
                    return EXIT_FAILURE;
                }

                for (; *pptr != NULL; pptr++) {
                    bzero(&server_addr6, sizeof (server_addr6));
                    server_addr6.sin6_family = AF_INET6;
                    server_addr6.sin6_port = htons(PORT);
                    memcpy(&server_addr6.sin6_addr, *pptr, sizeof(struct in6_addr));

                    inet_ntop(AF_INET6, (struct sockaddr *) *pptr, str6, sizeof (str6));
                    printf("\ttrying: %s\n", str6);

                    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof (server_addr)) == 0)
                        break;
                    fprintf(stderr, "IPV6 connect error : %s \n", strerror(errno));
                    close(client_sock);
                }
                break;
            default:
                fprintf(stderr, "unknown address type\n");
                break;
        }


        if (*pptr == NULL) {
            printf("unable to connect\n");
            return 1;
        }
    }

    if (client_sock < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(client_sock, SOCK_STREAM, SO_RCVTIMEO, (const char *) &timeout, sizeof (timeout));

    // Set send timeout (5 seconds)
    setsockopt(client_sock, SOCK_STREAM, SO_SNDTIMEO, (const char *) &timeout, sizeof (timeout));

    printf("Connected to server.\n");
    signal(SIGPIPE, sig_handler);
    int bytes;
    int received;
    char password[256];
    char buffer_send[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    int bytes_send;
    while (1) {
        if (loggon) {
            fprintf(stderr, "Enter command:");
            fgets(buffer_send, BUFFER_SIZE, stdin);
            buffer_send[strcspn(buffer_send, "\n")] = '\0'; // Remove newline

            if (strncmp(buffer_send, "UPLOAD", 6) == 0) {
                char filename[256];
                sscanf(buffer_send + 7, "%s", filename);
                memset(buffer_send, 0, BUFFER_SIZE);
                sprintf(buffer_send, "ls -ld %s", filename);

                FILE *fp = popen(buffer_send, "r");
                memset(buffer, 0, BUFFER_SIZE);
                fgets(buffer, sizeof (buffer), fp);
                fclose(fp);
                if (strncmp(buffer, "d", 1) == 0) {
                    memset(buffer_send, 0, BUFFER_SIZE);
                    sprintf(buffer_send, "zip -r %s.zip %s", filename, filename);
                    system(buffer_send);
                    sprintf(filename, "%s.zip", filename);
                }

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
                bytes_send = send(client_sock, buffer_send, strlen(buffer_send), 0);
                sleep(1);
                received = 0;
                while ((received = fread(buffer_send, sizeof(char), BUFFER_SIZE, file)) > 0) {
                    bytes_send = send(client_sock, buffer_send, received, 0);
                }

                fclose(file);
            } else if (strncmp(buffer_send, "LS", 2) == 0) {
                FILE *fp = popen("ls", "r");
                if (!fp) {
                    perror("ERROR Listing files");
                    continue;
                }

                memset(buffer, 0, BUFFER_SIZE);
                while (fgets(buffer, sizeof (buffer), fp)) {
                    fprintf(stderr, "%s", buffer);
                }

                sprintf(buffer_send, "%s", "CONTINUE");
                bytes_send = send(client_sock, buffer_send, strlen(buffer_send), 0);

                fclose(fp);
            } else if (strncmp(buffer_send, "SHOW", 4) == 0) {
                char filename[256];
                sscanf(buffer_send + 5, "%s", filename);
                memset(buffer_send, 0, BUFFER_SIZE);
                sprintf(buffer_send, "cat %s", filename);


                FILE *fp = popen(buffer_send, "r");


                if (!fp) {
                    perror("ERROR Showing file");
                    continue;
                }

                memset(buffer, 0, BUFFER_SIZE);
                while (fgets(buffer, sizeof (buffer), fp)) {
                    fprintf(stderr, "%s", buffer);
                }

                sprintf(buffer_send, "%s", "CONTINUE");
                bytes_send = send(client_sock, buffer_send, strlen(buffer_send), 0);


                fclose(fp);
            } else {
                send(client_sock, buffer_send, strlen(buffer_send), 0);
            }

            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if (strncmp(buffer_send, "QUIT", 4) == 0) {
                printf("Server response: \nBYE BYE <3\n Disconnecting...\n");
                break;
            } else if (strncmp(buffer, "READ", 4) == 0) {
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
                    received = 0;
                    while (received < filesize) {
                        bytes = recv(client_sock, buffer, BUFFER_SIZE, 0);
                        fwrite(buffer, sizeof(char), bytes, file);
                        received += bytes;
                    }

                    memset(buffer, 0, BUFFER_SIZE);
                    fclose(file);
                }
            } else {
                if (bytes_received > 0 && strncmp(buffer_send, "CONTINUE", 8) != 0) {
                    fprintf(stderr, "Server response: \n%s\n", buffer);
                }
            }
            if (bytes_received == 0) {
                fprintf(stderr, "Server terminated connection");
                exit(1);
            } else if (bytes_received < 0) {
                fprintf(stderr, "Something went wrong");
                exit(1);
            }
        } else {
            fprintf(stdout, "Please choose action: LOGIN, REGISTER:");
            fgets(buffer_send, BUFFER_SIZE, stdin);
            if (strncmp(buffer_send, "LOGIN", 5) == 0) {
                if (send(client_sock, buffer_send, strlen(buffer_send), 0) <= 0) {
                    fprintf(stderr, "Something went wrong");
                    exit(1);
                }
                fprintf(stdout, "Please login to a server, provide an user login:");
                fgets(buffer_send, BUFFER_SIZE, stdin);
                buffer_send[strcspn(buffer_send, "\n")] = '\0';
                fprintf(stderr, "Provide an user password:");
                fgets(password, sizeof (password), stdin);
                strcat(buffer_send, " ");
                strcat(buffer_send, password);
                if (send(client_sock, buffer_send, strlen(buffer_send), 0) <= 0) {
                    fprintf(stderr, "Something went wrong \n");
                    exit(1);
                }
                memset(buffer, 0, BUFFER_SIZE);
                if (bytes = recv(client_sock, buffer, BUFFER_SIZE, 0) <= 0) {
                    fprintf(stderr, "Something went wrong! \n");
                    exit(1);
                }
                if (strncmp(buffer, "LOGIN OK", 8) == 0) {
                    loggon = true;
                    fprintf(stderr,
                            "Available commands: SHOW [file], UPLOAD [file], LS, LIST, QUIT, DOWNLOAD [FILE], CAT [file], DELETE [file]. \n");
                }
                fprintf(stderr, buffer);
                fprintf(stderr, "\n");
            } else if (strncmp(buffer_send, "REGISTER", 8) == 0) {
                if (send(client_sock, buffer_send, strlen(buffer_send), 0) <= 0) {
                    fprintf(stderr, "Something went wrong");
                    exit(1);
                }
                fprintf(stdout, "Please register to a server, provide an user login:");
                fgets(buffer_send, BUFFER_SIZE, stdin);
                buffer_send[strcspn(buffer_send, "\n")] = '\0';
                fprintf(stderr, "Provide an user password:");
                fgets(password, sizeof (password), stdin);
                strcat(buffer_send, " ");
                strcat(buffer_send, password);
                if (send(client_sock, buffer_send, strlen(buffer_send), 0) <= 0) {
                    fprintf(stderr, "Something went wrong");
                    exit(1);
                }
                memset(buffer, 0, sizeof(buffer));
                if (bytes = recv(client_sock, buffer, BUFFER_SIZE, 0) <= 0) {
                    fprintf(stderr, "Something went wrong");
                    exit(1);
                }
                if ((strncmp(buffer, "REGISTER OK", 11) == 0)) {
                    loggon = true;
                    fprintf(stderr,
                            "Available commands: SHOW [file], UPLOAD [file], LS, LIST, QUIT, DOWNLOAD [FILE], CAT [file], DELETE [file]. \n");
                }
                fprintf(stderr, buffer);
                fprintf(stderr, "\n");
            } else if (strncmp(buffer_send, "QUIT", 4) == 0) {
                fprintf(stderr, "Disconnecting... \n");
                close(client_sock);
                break;
            } else {
                fprintf(stderr, "Error, unknown command.");
            }
        }
    }

    close(client_sock);
    return 0;
}
