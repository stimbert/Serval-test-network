/* sources used to create the program:
    daemon:
        https://openclassrooms.com/forum/sujet/creer-un-daemon-en-c-63619

    tcp server:
        https://www.binarytides.com/server-client-example-c-sockets-linux/
        https://github.com/mafintosh/echo-servers.c/blob/master/tcp-echo-server.c
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
#include <strings.h>
#include <netinet/in.h>
#include <dirent.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>

#define PORT_LISTEN 8800
#define BUFFER_SIZE 1024
#define RCVSIZE 1494
#define MAX_NB_CLIENT 10
#define SERIAL_NAME "/dev/ttyUSB0"

#include "radio.h"
#include "sync.h"
#include "lbard.h"
#include "serial.h"
#include "version.h"

int socketfd; //socket to exchange data

int debug_http = 0;
int debug_radio = 0;
int debug_pieces = 0;
int debug_bitmap = 0;
int debug_ack = 0;
int debug_bundles = 0;
int debug_announce = 0;
int debug_pull = 0;
int debug_insert = 0;
int debug_radio_rx = 0;
int debug_radio_tx = 0;
int debug_gpio = 0;
int debug_message_pieces = 0;
int debug_sync = 0;
int debug_sync_keys = 0;
int debug_noprioritisation = 0;
int debug_bundlelog = 0;
char *bundlelog_filename = NULL;

int fix_badfs = 0;

long long radio_last_heartbeat_time = 0;
int radio_temperature = 9999;

long long last_servald_contact = 0;

// If either of these is not -1, then we try to set them
// for the attached radio.
int txpower = -1;
int txfreq = -1;

char *otabid = NULL;
char *otadir = NULL;

char *onepeer = NULL;

int radio_silence_count = 0;

int http_server = 1;
int udp_time = 0;
int time_slave = 0;
int time_server = 0;
char *time_broadcast_addrs[] = {DEFAULT_BROADCAST_ADDRESSES, NULL};

int reboot_when_stuck = 0;
extern int serial_errors;

unsigned char my_sid[32];
unsigned char my_signingid[32];
char *my_sid_hex = NULL;
char *my_signingid_hex = NULL;
unsigned int my_instance_id;
time_t last_instance_time = 0;

char *servald_server = "";
char *credential = "";
char *prefix = "";

char *token = NULL;

time_t last_summary_time = 0;
time_t last_status_time = 0;

int monitor_mode = 0;

struct sync_state *sync_state = NULL;
int urandombytes(unsigned char *buf, size_t len)
{
    static int urandomfd = -1;
    int tries = 0;
    if (urandomfd == -1)
    {
        for (tries = 0; tries < 4; ++tries)
        {
            urandomfd = open("/dev/urandom", O_RDONLY);
            if (urandomfd != -1)
                break;
            sleep(1);
        }
        if (urandomfd == -1)
        {
            perror("open(/dev/urandom)");
            return -1;
        }
    }
    tries = 0;
    while (len > 0)
    {
        ssize_t i = read(urandomfd, buf, (len < 1048576) ? len : 1048576);
        if (i == -1)
        {
            if (++tries > 4)
            {
                perror("read(/dev/urandom)");
                if (errno == EBADF)
                    urandomfd = -1;
                return -1;
            }
        }
        else
        {
            tries = 0;
            buf += i;
            len -= i;
        }
    }
    return 0;
}

long long start_time = 0;

void crash_handler(int signal)
{
    fprintf(stderr, "SIGABORT intercepted. Exiting cleanly.\n");
    exit(0);
}

unsigned int option_flags = 0;

/* #define for debuging the code with printed text
if DEBUG=0, then printf will not be executed
If DEBUG!=0, then printf will be executed */
#define DEBUG 0

//#define ECHO_MODE

//Fonction for daemon mode
void daemonize()
{
    pid_t pid, sid;

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);
    else if (pid > 0)
        exit(EXIT_SUCCESS);
    //dad is leaving

    umask(0);
    /* default right 0777 */

    sid = setsid();
    /* setsid return the pid of dad but fail if the son process 
    has the same pid has an existing process */

    if (sid < 0)
    {
        perror("daemonize::sid");
        exit(EXIT_FAILURE);
    }

    /*if (chdir("/") < 0)
    {
        perror("daemonize::chdir");
        exit(EXIT_FAILURE);
    }*/

    //chdir("/");
    /* change the directory */

    //close(STDIN_FILENO);
    //close(STDOUT_FILENO);
    //close(STDERR_FILENO);
    /* le fils partage les descripteurs de fichier du père sauf si on les
    ferme et dans ce cas ceux du père ne seront pas fermés */
}

int main()
{

//launch deamon
//daemon is not link to the current TTY
//It doesn't have a stdout stdin and stderr (/dev/null)
#if DEBUG
    //in debug mode, we do not transform our process into a daemon
#else
    daemonize();
#endif

    //general socket parameters
    int port = PORT_LISTEN;
    int socketSrvListen; //socket to accept clients
    struct sockaddr_in client, server;
    int valid = 1;
    int listener = 1;    //to differentiate listener process and the ones in communication with clients
    int portData = 9000; //initial port for data communication
    char buffer[RCVSIZE];

    //create socket
    socketSrvListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketSrvListen < 0)
    {
#if DEBUG
        perror("Socket error: cannot create the socket for the server!\n");
#endif
        return -1;
    }
#if DEBUG
    printf("Server socket successfully created\n");
#endif

    setsockopt(socketSrvListen, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind
    if (bind(socketSrvListen, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
//print the error message
#if DEBUG
        perror("bind failed. Error\n");
#endif
        close(socketSrvListen);
        return -1;
    }
#if DEBUG
    printf("bind successful\n");
#endif

    //listen
    int err = listen(socketSrvListen, MAX_NB_CLIENT);

    //while loop for the code
    while (1)
    {

        if (listener)
        {
            //code for the process accepting clients
            /*
        //opening new socket for the data
        socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if ( socketfd < 0 ) {
            #if DEBUG
                perror("Unable to create socket for data\n");
            #endif
            return -1;
        }
        #if DEBUG
            printf("Socket for data successfully created\n");
        #endif

        client.sin_family=AF_INET;
        client.sin_port = portData;
        client.sin_addr.s_addr = htonl(INADDR_ANY);


        if (bind(socketfd, (struct sockaddr*) &client, sizeof(client)) == -1) {
            #if DEBUG
                perror("Bind for data socket failed\n");
            #endif
            close(socketfd);
            return -1;
        }
        */
            socklen_t cli_addr_size = sizeof(client);
            socketfd = accept(socketSrvListen, (struct socketaddr *)&client, &cli_addr_size);
            if (socketfd < 0)
            {
#if DEBUG
                perror("accept fail\n");
#endif
                return -1;
            }

            //fork
            pid_t pid = fork();
            if (pid < 0)
            {
#if DEBUG
                perror("Error on fork\n");
#endif
                return -1;
            }
            else if (pid == 0)
            {
                //son part
                listener = 0;

#if DEBUG
                struct hostent *hostp = gethostbyaddr((const char *)&client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), AF_INET);
                if (hostp == NULL)
                {
                    perror("Could not get the host address\n");
                    return -1;
                }
                char *hostaddrp = inet_ntoa(client.sin_addr);
                if (hostaddrp == NULL)
                {
                    perror("Error inet_ntoa\n");
                    return -1;
                }
                printf("The server has established a communication with %s (%s)\n", hostp->h_name, hostaddrp);
#endif
                rfd_monitor_handler();
            }
            else
            {
                //dad part
            }
        }
        else
        {
            // test code for sending data to the client
#ifdef ECHO_MODE
            if (recv(socketfd, buffer, RCVSIZE, 0) < 0)
            {
                perror("Could not receive\n");
                return -1;
            }

            printf("Message received: %s\n", buffer);
            char answer[RCVSIZE];
            strcpy(answer, buffer);

            printf("Sending answer: %s\n", answer);

            if (send(socketfd, answer, RCVSIZE, 0) < 0)
            {
                perror("Could not send the answer");
                return -1;
            }
            printf("closing");
            close(socketfd);
            return 0;
#endif

//code for sending data to the client
#ifndef ECHO_MODE

#endif
        }
    }
}

/* Fonction connection: only useful for udp
Fonction used to initiate the connection with a client
This fonction :
    - tells the client the port to connect on the data socket.
    - create a fork
*/
/* not used (only for udp)
int connection(int socketDesc, struct sockaddr *from, int fromsize, int portD){
    char buffer[20];
    

    //send port number to the client
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "PORT%d\0", portD);
    #if DEBUG
        printf("Sending port number to the client: %s\n", buffer);
    #endif

    if(sendto(socketDesc, buffer, strlen(buffer), 0, from, fromsize) <0){
        #if DEBUG
            perror("Fail to send port to client\n");
        #endif
        return -1;
    }

    //wait for ACK
    memset(buffer, 0, sizeof(buffer));
    int msgSize = recvfrom(socketDesc, buffer, RCVSIZE, 0, from, &fromsize);
    if( msgSize < 0){
        #if DEBUG
            perror("Fail to receive\n");
        #endif
    }

    if(strcmp(buffer, "ACK") != 0){
        #if DEBUG
            printf("No ACK receive. Received message: %s\n", buffer);
        #endif
        return -1;
    }

    return 0;
}*/

void comm_handler(int socket)
{
    int a, b, c;
    a = dup2(socketfd, 1);
    b = dup2(socketfd, 2);
    //c = dup2(socketfd, 0);
    if (a < 0 || b < 0 || c < 0)
    {
        send_all("ERROR: could not duplicate the socket file handle: \n");
        exit(1);
    }
    sleep(2);
    char socketMessage[100];
    snprintf(socketMessage, 100, "azerty %d %d %d\n", c, a, b);
    send_all(socketMessage);
    fprintf(stderr, "HELLO me!\n");
}

void rfd_monitor_handler()
{

//redirect stdout and stderr
#if DEBUG
    //do nothing in DEBUGE mode
#else
    comm_handler(socketfd);
#endif
    //setup poll
    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = socketfd;
    fds[0].events = POLLIN;
    int timeout = 100; //timer for poll in ms
    int rc, nfds = 1;

    //Getting the serial port ready

    start_time = gettime_ms();

    // Ignore broken socket connections
    signal(SIGPIPE, SIG_IGN);

    /* Catch SIGABORT, for compatibility with test framework (expects return code 0
     on SIGSTOP */
    struct sigaction sig;
    sig.sa_handler = crash_handler;
    sigemptyset(&sig.sa_mask);                // Don't block any signals during handler
    sig.sa_flags = SA_NODEFER | SA_RESETHAND; // So the signal handler can kill the process by re-sending the same signal to itself
    sigaction(SIGABRT, &sig, NULL);
    sig.sa_handler = crash_handler;
    sigemptyset(&sig.sa_mask);                // Don't block any signals during handler
    sig.sa_flags = SA_NODEFER | SA_RESETHAND; // So the signal handler can kill the pro
    sigaction(SIGSTOP, &sig, NULL);

    // Setup random seed, so that multiple LBARD's started at the same time
    // can't easily end up in lock step.
    uint32_t seed;
    // Seed generator
    urandombytes((unsigned char *)&seed, sizeof(uint32_t));
    srandom(seed);
    // Then skip 0 - 4095 initial values, so that even identical seeds won't
    // easily cause problems
    urandombytes((unsigned char *)&seed, sizeof(uint32_t));
    seed &= 0xfff;
    while (seed--)
        random();

    sync_setup(); //STEPHANE comment: MANDATORY

    // Generate a unique transient instance ID for ourselves.
    // Must be non-zero, as we use zero as a marker for not having yet heard the
    // instance ID of a peer.
    my_instance_id = 0;
    while (my_instance_id == 0)
        urandombytes((unsigned char *)&my_instance_id, sizeof(unsigned int));
    last_instance_time = time(0);

    //fprintf(stderr, "Version commit:%s branch:%s [MD5: %s] @ %s\n",
    //        GIT_VERSION_STRING, GIT_BRANCH, VERSION_STRING, BUILD_DATE);

    char *serial_port = SERIAL_NAME;
    monitor_mode = 1;
    debug_pieces = 1;

    if (message_update_interval < 0)
        message_update_interval = 0;

    last_message_update_time = 0;
    congestion_update_time = 0;

    my_sid_hex = "000000000000000000000000000000000";
    prefix = "000000";
    my_signingid_hex = "0000000000000000000000000000000";

    int serialfd = -1;
    serialfd = open(serial_port, O_RDWR);
    if (serialfd < 0)
    {
#if DEBUG
        perror("Opening serial port in main");
#endif
        exit(-1);
    }
    //to check
    if (serial_setup_port(serialfd))
    {
#if DEBUG
        fprintf(stderr, "Failed to setup serial port. Exiting.\n");
#endif
        exit(-1);
    }

    int mode = 1; // starting on UHF mode
    do
    {

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
            // we are using a mode variable because we are not sure we are going through  this part of the code
            char socketBuffer[100]; // we can only receive short message
            //something to read the socket
            rc = recv(fds[0].fd, socketBuffer, 100, 0);
            if (rc < 0)
            {
                fprintf(stderr, "Server Could not receive the data\n");
                break;
            }

            if (strcmp(socketBuffer, "CLOSE") == 0)
            {
                mode = 0;
#if DEBUG
                printf("CLOSE MESSAGE RECEIVEDn");
#endif
            }
            else if (strcmp(socketBuffer, "UHF") == 0)
            {
                mode = 1;
            }
            else if (strcmp(socketBuffer, "WIFI") == 0)
            {
                mode = 2;
            }
            else
            {
                // default case
            }
        }

        if (mode == 1)
        {
            radio_read_bytes(serialfd, monitor_mode);
        }
    } while (mode != 0);

#if DEBUG
    printf("Ending connection with client\nGoodbye\n");
#endif
}
