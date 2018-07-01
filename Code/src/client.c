/* sources used to create the program:
    daemon:
        https://openclassrooms.com/forum/sujet/creer-un-daemon-en-c-63619

    tcp client:
        https://www.binarytides.com/server-client-example-c-sockets-linux/
        http://www.cs.tau.ac.il/~eddiea/samples/IOMultiplexing/TCP-client.c.html

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include "library.h"
#include <sys/poll.h>

#define PORT_LISTEN 8800
#define BUFFER_SIZE 1024
#define RCVSIZE 1494
#define MAX_NB_CLIENT 10
#define CLOSE_MESSAGE "CLOSE"

#define FILE_NAME "hostnames"

/* #define for debuging the code with printed text
if DEBUG=0, then printf for debug will not be executed
If DEBUG!=0, then printf will be executed */
#define DEBUG 1

//#define ECHO_MODE
int socketSrv = -1;
int checkConnect = -1;
int fd;
int logMode = 0;
char fileLog[100];
FILE *file;
void execute_order(char order[]);
char **split_line(char *line);
static void catch_sighup(int signpo)
{
    //check socketSrv
    if (checkConnect >= 0)
    {
        printf("SENDING CLOSE MESSAGE .......................................................\n");
        for (int i = 0; i < 100; i++)
            send(socketSrv, CLOSE_MESSAGE, RCVSIZE, 0);
    }
    exit(0);
}

int main(int argc, char *argv[])
{

    signal(SIGHUP, catch_sighup);
    signal(SIGINT, catch_sighup);
    char buffer[BUFFER_SIZE];
    struct hostent *he;
    struct sockaddr_in server;

    if (argc < 2 || argc > 3)
    {
        perror("Wrong parameters, usage: ./client hostname\n");
        return -1;
    }

    if (argc == 3)
    {
        printf("opening fifo: %s\n", argv[2]);
        fd = open(argv[2], O_RDONLY);
        printf("fd : %d\n", fd);
        sleep(2);
    }
    else
    {
        fd = STDIN_FILENO;
    }
    //find available servers using their hostnames:
    //ArrayHostent hostList = findServers();
    /*char test[100];
    read(fd, test, 100);
    printf("%s\n", test);
    sleep(3);
*/
    he = gethostbyname(argv[1]);
    if (he == NULL)
    {
        perror("Couldn't find the host\n");
        return -1;
    }
    printf("The server was found at %s (%s)\n", he->h_name, inet_ntoa(*((struct in_addr *)he->h_addr_list[0])));

    socketSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketSrv < 0)
    {
        perror("Could not create socket\n");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT_LISTEN);
    server.sin_addr = *((struct in_addr *)he->h_addr_list[0]);

    checkConnect = connect(socketSrv, (struct sockaddr *)&server, sizeof(struct sockaddr));
    if (checkConnect < 0)
    {
        perror("Could not connect to the server\n");
        return -1;
    }

    printf("Connection with the server successful\n");

    //setup poll
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));
    fds[1].fd = socketSrv;
    fds[1].events = POLLIN;
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    int timeout = 100; //timer for poll in ms
    int rc, nfds = 2;
    while (1)
    {

#ifdef ECHO_MODE
        //test echo
        printf("Sending an echo to the server: Hello World! \n");
        sprintf(buffer, "Hello World!");
        if (send(socketSrv, buffer, 13, 0) < 0)
        {
            perror("Could send hello world");
            return -1;
        }
        printf("Waiting for Server answer ...\n");
        memset(buffer, 0, sizeof(buffer));
        int numbytes = recv(socketSrv, buffer, BUFFER_SIZE, 0);
        if (numbytes < 0)
        {
            perror("Could not receive the server answer.\n");
            return -1;
        }

        printf("Answer of the server: %s\n", buffer);
        printf("Closing\n");
        close(socketSrv);
        printf("closed\n");
        return 0;
#else

        //simply read data and print them
        rc = poll(fds, nfds, timeout);
        if (rc < 0)
        {
            break;
        }
        else if (rc == 0)
        {
            //timout
        }
        else
        {

            for (int i = 0; i < nfds; i++)
            {
                if (fds[i].revents == 0)
                    continue; //nothing to read

                if (fds[i].fd == fd)
                {

                    //information coming from client_shell
                    char order[100];
                    read(fds[i].fd, order, 100);
                    if (order[strlen(order) - 1] == '\n')
                        order[strlen(order) - 1] = '\0';
                    printf("Order received: %s......................\n", order);
                    sleep(1);
                    execute_order(order);
                }

                if (fds[i].fd != STDIN_FILENO)
                {
                    memset(buffer, 0, sizeof(buffer));
                    int numbytes = recv(socketSrv, buffer, BUFFER_SIZE, 0);
                    if (numbytes < 0)
                    {
                        perror("Could not receive the server answer.\n");
                        return -1;
                    }
                    printf("%s", buffer);
                    if(logMode){
                        fprintf(file, "%s", buffer);
                    }
                }
            }
        }
#endif
    }
    close(socketSrv);
    return 0;
}

ArrayHostent *findServers()
{

    char line[100];

    //open file with hostnames
    FILE *file = NULL;
    file = fopen(FILE_NAME, "r");
    if (file == 0)
    {
        printf("Fail to open file: %s\n", FILE_NAME);
        exit(1);
    }
    printf("File correctly opened\n");

    //create dynamic array
    ArrayHostent *hostList;
    //hostList = initArray();
    hostList = malloc(sizeof(ArrayHostent));
    //hostList->hostname = NULL;
    //hostList->address = 0;
    hostList->next = NULL;
    hostList->index = 0;

    //For each hostname check and add it to the struct
    while (fgets(line, sizeof(line), file) != NULL)
    {
        //removing \n
        int len = strlen(line);
        if (line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
        }
#if DEBUG
        printf("line is: %s\n", line);
#endif
        struct hostent *he = gethostbyname(line);
        if (he == NULL)
        {
#if DEBUG
            printf("%s was not found\n", line);
#endif
        }
        else
        {
#if DEBUG
            printf("%s was found\n", he->h_name);
            printf("Adding %s to the list\n", line);
#endif
            hostList = insertArray(hostList, *he);
        }
    }

    //display all the host in ArrayHostent
    ArrayHostent *current;
    current = hostList;
    while (current->next != NULL)
    {
        printf("The server %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));
        current = current->next;
    }
    printf("The server %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));

    //close file
    fclose(file);
    return hostList;
}

void execute_order(char order[])
{

    char **args;
    args = split_line(order);
    
    if (strcmp(args[0], "CLOSE") == 0)
    {
        catch_sighup(1);
    }
    else if (strcmp(args[0], "LOG") == 0)
    {
        logging(args);
    }
    else
    {
        printf("Could not understand the following order: '%s' (length:%d)\n", order, strlen(order));
    }
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens)
    {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;

        if (position >= bufsize)
        {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens)
            {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int logging(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(STDERR_FILENO, "Not enough argument for logging\n");
        return -1;
    }

    if (args[1] == "STOP")
    {
        if (logMode == 1)
        {
            printf("Closing log file: %s\n", fileLog);
            logMode = 0;
            fclose(file);
        }
    }
    else
    {
        if (logMode == 1)
        {
            fclose(file);
            printf("Closing previous log file: %s\n", fileLog);
        }
        strcpy(fileLog, args[1]);
        file = fopen(fileLog,"a");
        printf("New log file %s opened\n", fileLog);
        logMode = 1;
    }

}