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
int lsh_close_client(char **args);
int lsh_list_clients(char **args);
int lsh_log(char **args);
void DisplayServers(ArrayHostent *hostList);
void list_client();

/*
typedef struct clients {
    int fd[2];
    int number_client;    
} clients;
*/
typedef struct clients {
    int fd;
    char fifo[100];
    int number_client;    
} clients;


int add_client();
int check_number_client();
int remove_client(int index);
int close_client(int index);
int close_all_client();
int initiate_client();
int log_client(int index, char *filename);


typedef struct builtin
{
  char *builtin_str;
  int (*builtin_func)(char **);
} builtin;

builtin functionList[] = {
    {"cd", &lsh_cd},
    {"help", &lsh_help},
    {"exit", &lsh_exit},
    {"launch", &lsh_server_launch},
    {"findServers", &lsh_find_server},
    {"displayServers", &lsh_display_server},
    {"close", &lsh_close_client},
    {"list", &lsh_list_clients},
    {"log", &lsh_log}};