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

#include "library.h"


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
    
}

ArrayHostent *insertArray(ArrayHostent *head, struct hostent element)
{
    ArrayHostent *current = head;
    struct in_addr y = *((struct in_addr *)element.h_addr_list[0]);
    if (strlen(current->hostname) == 0)
    {
        //current->hostname = malloc(sizeof(char)*strlen(element.h_name));
        strcpy(current->hostname, element.h_name);
        current->address = y;
    }
    else
    {
        while (current->next != NULL)
        {
            current = current->next;
        }

        current->next = malloc(sizeof(ArrayHostent));
        if (current->next == NULL)
        {
            printf("ERROR\n");
        }
        
        strcpy(current->next->hostname, element.h_name);
        current->next->next = NULL;
        current->next->index = current->index + 1;
        current->next->address = y;
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

