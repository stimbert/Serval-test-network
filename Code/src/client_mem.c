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

#include "client.h"

#define PORT_LISTEN 8800
#define BUFFER_SIZE 1024
#define RCVSIZE 1494
#define MAX_NB_CLIENT 10

#define FILE_NAME "hostnames"

/* #define for debuging the code with printed text
if DEBUG=0, then printf for debug will not be executed
If DEBUG!=0, then printf will be executed */
#define DEBUG 1

#define ECHO_MODE

int main(int argc, char *argv[])
{

    int socketSrv;
    char buffer[BUFFER_SIZE];
    struct hostent *he;
    struct sockaddr_in server;

    if (argc != 2)
    {
        perror("Wrong parameters, usage: ./client hostname\n");
        return -1;
    }

    //find available servers using their hostnames:
    ArrayHostent hostList = findServers();
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

    if (connect(socketSrv, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0)
    {
        perror("Could not connect to the server\n");
        return -1;
    }

    printf("Connection with the server successful\n");

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
#endif
    }
}

ArrayHostent findServers()
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
        printf("The LOOr %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));
        current = current->next;
    }
    printf("The LOOr %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));

    //close file
    fclose(file);
}

ArrayHostent *initArray()
{
    ArrayHostent *a = (struct ArrayHostent *)malloc(sizeof(ArrayHostent));
    if (a == NULL)
    {
        perror("Could not allocate memory for the array");
        return NULL;
    }
    a->index = 0;
    a->next = NULL;
    //a->hostname = NULL;
    return a;
    /*
    a->he = (struct hostent *)malloc(sizeof(struct hostent));
    if (a->he == NULL){
        perror("Could not allocate memory for the array");
        return -1;
    }
    a->used = 0;
    a->size = initialSize;
    */
}

ArrayHostent *insertArray(ArrayHostent *head, struct hostent element)
{
    ArrayHostent *current = head;
    struct in_addr y = *((struct in_addr *)element.h_addr_list[0]);
    printf("a%d\n", strlen(element.h_name));
    if (strlen(current->hostname) == 0)
    {
        //current->hostname = malloc(sizeof(char)*strlen(element.h_name));
        printf("a%d\n", strlen(element.h_name));
        strcpy(current->hostname, element.h_name);
        printf("jgifjklgjkldf %s\n", current->hostname);
        current->address = y;
    }
    else
    {
        while (current->next != NULL)
        {
            printf("jgifjklgjkldf %s\n", current->hostname);
            current = current->next;
        }

        printf("HI: %s\n", element.h_name);
        current->next = malloc(sizeof(ArrayHostent));
        if (current->next == NULL)
        {
            printf("ERROR\n");
        }
        //current->next->hostname = malloc(sizeof(char)*strlen(element.h_name));

        //current->next->hostname = NULL;
        printf("hdkjfhj %d\n", current->next);

        //current->next->he = malloc(sizeof(struct hostent));
        //printf("Im getting mad: %s\n", current->next->hostname);
        strcpy(current->next->hostname, element.h_name);
        printf("Hehe: %s\n", current->next->hostname);
        current->next->next = NULL;
        current->next->index = current->index + 1;
        current->address = y;
        //printf("HEH:%d - %d", current->inde)
    }
    return head;

    /*
    if (a->used == a->size){
        a->size += 10;
        a->he = (struct hostent *)realloc(a->he, a->size * sizeof(struct hostent));
        if (a->he == NULL){
            perror("Could not reallocate memory");
            freeArray(a);
            return -1;
        }
    }
    *(a->he[a->used++])=element;
    return 0;
    */
}

int freeArray(ArrayHostent *a)
{

    if (a->next == NULL)
    {
        free(a);
        return 0;
    }
    freeArray(a->next);
    /*
    free(a->he);
    a->he = NULL;
    a-> used = 0;
    a->size = 0;
    return 0;
    */
}