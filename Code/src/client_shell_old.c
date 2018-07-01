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

#include <sys/wait.h>

#include "library.h"

#define PORT_LISTEN 8800
#define BUFFER_SIZE 1024
#define RCVSIZE 1494
#define MAX_NB_CLIENT 10

#define FILE_NAME "hostnames"

/* #define for debuging the code with printed text
if DEBUG=0, then printf for debug will not be executed
If DEBUG!=0, then printf will be executed */
#define DEBUG 1

void lsh_loop(void);
char *lsh_read_line(void);

char **lsh_split_line(char *line);
int lsh_launch(char **args);
/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_server_launch(char **args);
int lsh_execute(char **args);
int launch_client(char *name);
int lsh_display_server(char **args);
int lsh_find_server(char **args);
void DisplayServers(ArrayHostent *hostList);

ArrayHostent *hostList;

int main(int argc, char *argv[])
{
  //initialisation
  /*
    int pid=fork();
    if (pid==0){
    execl("/usr/bin/xterm", "xterm", "-e", "/home/stephane/Documents/ServalStephane/client localhost;bash", NULL);
    }else{
    printf("YO\n");
    }
    */
  hostList = findServers();

  DisplayServers(hostList);

  lsh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}

void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do
  {
    printf("######> ");
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

#define LSH_RL_BUFSIZE 1024

char *lsh_read_line(void)
{
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  getline(&line, &bufsize, stdin);
  return line;
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line)
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

int lsh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0)
  {
    // Child process
    if (execvp(args[0], args) == -1)
    {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  }
  else if (pid < 0)
  {
    // Error forking
    perror("lsh");
  }
  else
  {
    // Parent process
    do
    {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/*
  List of builtin commands, followed by their corresponding functions.
 */
typedef struct builtin{
  char *builtin_str;
  int (*builtin_func)(char **);
} builtin;

builtin functionList[] = {
  {"cd", &lsh_cd},
  {"help", &lsh_help},
  {"exit", &lsh_exit},
  {"launch", &lsh_server_launch},
  {"findServers", &lsh_find_server},
  {"displayServers", &lsh_display_server}
};

char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "launch",
    "findServers",
    "displayServers"
};

int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_server_launch,
    &lsh_find_server,
    &lsh_display_server 
};

int lsh_num_builtins()
{
  //return sizeof(builtin_str) / sizeof(char *);
  return sizeof(functionList) / (sizeof(char *)+sizeof(int *));
}

/*
  Builtin function implementations.
*/
int lsh_cd(char **args)
{
  if (args[1] == NULL)
  {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  }
  else
  {
    if (chdir(args[1]) != 0)
    {
      perror("lsh");
    }
  }
  return 1;
}

int lsh_help(char **args)
{
  int i;
  //printf("Stephen Brennan's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++)
  {
    //printf("  %s\n", builtin_str[i]);
    printf("  %s\n", functionList[i].builtin_str);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

int lsh_exit(char **args)
{
  return 0;
}

int lsh_server_launch(char **args)
{
  if (args[1] == NULL)
  {
    printf("lsh expected 1 argument\n");
    printf("There are two ways to use this command:\n");
    printf("\tlaunch index\n");
    printf("\tlaunch adress\n");
    return 1;
  }

  //check if args[1] is a number
  char *tab = args[1];
  int j = 0;
  while (j < strlen(tab))
  {
    if (tab[j] < '0' || tab[j] > '9')
    {
      //it's not a number
      //check if the host exist
      struct hostent *he = gethostbyname(args[1]);
      if (he == NULL)
      {
        printf("Could not find %s\n", args[1]);
        return 0;
      }
      else
      {
#if DEGUB
        printf("Argument for launch was:%s\n", args[1]);
#endif
        return launch_client(args[1]);
      }
    }
    j++;
  }

  //if it get out the while than tab is a number
  int index = atoi(args[1]);
  ArrayHostent *current;
  current = hostList;
  while (current->next != NULL)
  {
    if (current->index == index)
    {
      return launch_client(current->hostname);
    }
    current = current->next;
  }
  if (current->index == index)
  {
    return launch_client(current->hostname);
  }
}

int launch_client(char *name)
{
  char addr[300];
  sprintf(addr, "./client %s", name);

  int pid = fork();
  if (pid < 0)
  {
    printf("error in launch server forking\n");
    return 0;
  }
  else if (pid == 0)
  {
    //child
    execl("/usr/bin/xterm", "xterm", "-e", addr, NULL);
    exit(1);
  }
  else
  {
    return 1;
  }
}


int lsh_display_server(char **args){
  DisplayServers(hostList);
  return 1;
}


int lsh_find_server(char **args){
  ArrayHostent *new;
  freeArray(hostList);
  new = findServers();
  hostList = new;
  if (hostList == NULL){
    printf("Erreur while finding servers\n");
    return 0;
  }else
    return 1;
}





int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL)
  {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++)
  {
    if (strcmp(args[0], functionList[i].builtin_str) == 0)
    {
      return (*functionList[i].builtin_func)(args);
    }
  }

  return lsh_launch(args);
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
  ArrayHostent *host;
  //host = initArray();
  host = malloc(sizeof(ArrayHostent));
  //host->hostname = NULL;
  //host->address = 0;
  host->next = NULL;
  host->index = 0;

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
      host = insertArray(host, *he);
    }
  }

  //display all the host in ArrayHostent
  ArrayHostent *current;
  current = host;
  while (current->next != NULL)
  {
    printf("The server %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));
    current = current->next;
  }
  printf("The server %d was found at %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));

  //close file
  fclose(file);
  return host;
}

void DisplayServers(ArrayHostent *host)
{
  ArrayHostent *current;
  current = host;
  printf("You can choose the following servers using their index\n");
  while (current->next != NULL)
  {
    printf("%d: %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));
    current = current->next;
  }
  printf("%d: %s (%s)\n", current->index, current->hostname, inet_ntoa(current->address));
}

