// I pledge my honor that I have abided by the Stevens Honor System.
// Breanna Shinn
// Jose de la Cruz

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "util.h"

int client_socket = -1;
char username[MAX_NAME_LEN + 1];
char inbuf[BUFLEN + 1];
char outbuf[MAX_MSG_LEN + 1];
char ip_address[sizeof(struct in_addr)];
int port = 0;
int retval = EXIT_SUCCESS;
fd_set sockset;

struct sockaddr_in server_addr;
socklen_t addrlen = sizeof(struct sockaddr_in);

int handle_stdin()
{
    if (get_string(outbuf, MAX_MSG_LEN) == TOO_LONG)
    {
        fprintf(stderr, "Sorry, limit your message to %d characters.\n", MAX_MSG_LEN);
        memset(outbuf, 0, sizeof(outbuf));
    }

    if (send(client_socket, outbuf, strlen(outbuf), 0) < 0)
    {
        fprintf(stderr, "Error: Failed to send message to server. %s.\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    if (strcmp(outbuf, "bye") == 0)
    {
        printf("Goodbye.\n");
        return EXIT_FAILURE;
    }
    fflush(stdout);
    fflush(stdin);
    return EXIT_SUCCESS;
}

int handle_client_socket()
{
    int byte_recvd;
    if ((byte_recvd = recv(client_socket, inbuf, BUFLEN, 0)) < 0)
    {
        fprintf(stderr, "Warning: Failed to receive incoming message. %s.\n",
        strerror(errno));
    }
    else if (byte_recvd == 0)
    {
        fprintf(stderr, "\nConnection to server has been lost.\n");
        return EXIT_FAILURE;
    }
    else
    {
        inbuf[byte_recvd] = '\0';
        if (strcmp(inbuf, "bye") == 0)
        {
            printf("\nServer initiated shutdown.\n");
            return EXIT_FAILURE;
        }
        printf("\n%s\n", inbuf);
        fflush(stdout);
        fflush(stdin);
        return EXIT_SUCCESS;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    // Commmand-line Arguments
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server IP> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) < 1)
    {
        fprintf(stderr, "Error: Invalid IP address '%s'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (parse_int(argv[2], &port, "port number") == false)
    {
        return EXIT_FAILURE;
    }
    if ((port < 1024) || (port > 65535))
    {
        fprintf(stderr, "Error: Port must be in range [1024, 65535].\n");
        return EXIT_FAILURE;
    }
    // Prompting for a Username
    server_addr.sin_port = htons(port);
    int charsRead;
    while (strlen(username) <= 0)
    {
        printf("Enter your username: ");
        fflush(stdout);
        if ((charsRead = read(STDIN_FILENO, username, MAX_NAME_LEN + 2)) > 0)
        {
            if ((charsRead - 1) > MAX_NAME_LEN)
            {
                printf("Sorry, limit your username to %d characters.\n", MAX_NAME_LEN);
                fflush(stdout);
                while (getchar() != '\n')
                {
                }
                memset(username, 0, sizeof(username));
                continue;
            }
            else if (charsRead == 0)
            {
                continue;
            }
            else
            {
                username[charsRead - 1] = '\0';
            }
        }
    }
    // Establishing Connection
    printf("Hello, %s. Let's try to connect to the server.\n", username);

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Error: Failed to create socket. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr, addrlen) < 0)
    {
        close(client_socket);
        fprintf(stderr, "Error: Failed to connect to server. %s.\n", strerror(errno));
        fflush(stdout);
        retval = EXIT_FAILURE;
        goto EXIT;
    }
    int bytes_recvd;
    if ((bytes_recvd = recv(client_socket, inbuf, BUFLEN, 0)) < 0)
    {
        fprintf(stderr, "Error: Failed to receive message from server. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }
    else if (bytes_recvd == 0)
    {
        fprintf(stderr, "All connections are busy. Try again later.\n");
        retval = EXIT_FAILURE;
        goto EXIT;
    }
    inbuf[bytes_recvd] = '\0';
    printf("\n%s\n\n", inbuf);
    fflush(stdout);

    if (send(client_socket, username, strlen(username), 0) < 0)
    {
        fprintf(stderr, "Error: Failed to send username to server. %s.\n",
                strerror(errno));
        retval = EXIT_FAILURE;
        goto EXIT;
    }

    memset(inbuf, 0, BUFLEN + 1);
    memset(outbuf, 0, MAX_MSG_LEN + 1);

    // Handling Activity on File Descriptors (Sockets)

    int running = 1;
    while (running)
    {
        printf("[%s]: ", username);
        fflush(stdout);
        FD_ZERO(&sockset);
        FD_SET(STDIN_FILENO, &sockset);
        FD_SET(client_socket, &sockset);
        memset(inbuf, 0, BUFLEN + 1);
        memset(outbuf, 0, MAX_MSG_LEN + 1);

        select(client_socket + 1, &sockset, NULL, NULL, NULL);

        if (FD_ISSET(client_socket, &sockset))
        {
            if (handle_client_socket() == EXIT_FAILURE)
            {
                retval = EXIT_FAILURE;
                goto EXIT;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &sockset))
        {
            if (handle_stdin() == EXIT_FAILURE)
            {
                retval = EXIT_FAILURE;
                goto EXIT;
            }
        }
    }

    return retval;

EXIT:
    if (fcntl(client_socket, F_GETFD) >= 0)
    {
        close(client_socket);
        return retval;
    }
}