
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

#include "send_all.h"

extern int socketfd;



int send_all(char buffer[])
{
  size_t length = strlen(buffer);
  char *ptr = buffer;
  while (length > 0)
  {
    int i = send(socketfd, ptr, length, 0);
    if (i < 1)
    {
      return -1;
    }
    ptr = ptr + i;
    length = length - i;
  }
}