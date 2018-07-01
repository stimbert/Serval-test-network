
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

#include "radio.h"
#include "sync.h"
#include "lbard.h"
#include "serial.h"
#include "version.h"

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


int main(int argc, char **argv)
{

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
 
 char *serial_port = "/dev/null";

  if ((argc == 3) && ((!strcasecmp(argv[1], "monitor")) ||
                      (!strcasecmp(argv[1], "monitorts"))))
  {
    if (!strcasecmp(argv[1], "monitorts"))
    {
      time_server = 1;
      udp_time = 1;
    }
    monitor_mode = 1;
    debug_pieces = 1;
    debug_message_pieces = 1;
    serial_port = argv[2];
  }else{
      fprintf(stderr, "usage: lbard monitor <serial port>\n");
  }


  if (message_update_interval < 0)
    message_update_interval = 0;

  last_message_update_time = 0;
  congestion_update_time = 0;

  my_sid_hex = "000000000000000000000000000000000";
  prefix = "000000";
  my_signingid_hex = "0000000000000000000000000000000";



  fprintf(stderr, "%d:My SigningID as hex is %s\n", __LINE__, my_signingid_hex);
  fprintf(stderr, "%d:My SID as hex is %s\n", __LINE__, my_sid_hex);
  fprintf(stderr, "My SID prefix is %02X%02X%02X%02X%02X%02X\n",
          my_sid[0], my_sid[1], my_sid[2], my_sid[3], my_sid[4], my_sid[5]);
 

 //STEPHANE : CHECK opening serial port
  int serialfd = -1;
  serialfd = open(serial_port, O_RDWR);
  if (serialfd < 0)
  {
    perror("Opening serial port in main");
    exit(-1);
  }

  if (serial_setup_port(serialfd))
  {
    fprintf(stderr, "Failed to setup serial port. Exiting.\n");
    exit(-1);
  }
  fprintf(stderr, "Serial port open as fd %d\n", serialfd);

  int n = 6;
  while (n < argc)
  {
    if (argv[n])
    {
      if (!strcasecmp("monitor", argv[n]))
        monitor_mode = 1;
    }
    n++;
  }


  
  while (1)
  {
/*
    unsigned char msg_out[LINK_MTU];

    account_time("ID regenerate");

    // Refresh our instance ID every four minutes, so that any bundle list sync bugs
    // can only block transmission for a few minutes.
    if ((time(0) - last_instance_time) > 240)
    {
      my_instance_id = 0;
      while (my_instance_id == 0)
        urandombytes((unsigned char *)&my_instance_id, sizeof(unsigned int));
      last_instance_time = time(0);
    }

    account_time("radio_read_bytes()");
*/
    radio_read_bytes(serialfd, monitor_mode);
/*
    account_time("load_rhizome_db_async()");

    load_rhizome_db_async(servald_server,
                          credential, token);

    account_time("make_periodic_requests()");

    make_periodic_requests();

    account_time("radio.serviceloop()");

    if (radio_get_type() >= 0)
    {
      if (!radio_types[radio_get_type()].serviceloop)
      {
        fprintf(stderr, "Radio type set to illegal value %d\n",
                radio_get_type());
        exit(-1);
      }
      radio_types[radio_get_type()].serviceloop(serialfd);
    }
    else
    {
      fprintf(stderr, "ERROR: Connected to unknown radio type.\n");
      exit(-1);
    }

    account_time("time server: reverse timeflow check");

    // Deal gracefully with clocks that run backwards from time to time.
    if (last_message_update_time > gettime_ms())
      last_message_update_time = gettime_ms();

    account_time("time server: announce ");
    if ((gettime_ms() - last_message_update_time) >= message_update_interval)
    {

      if (!time_server)
      {
        // Decay my time stratum slightly
        if (my_time_stratum < 0xffff)
          my_time_stratum++;
      }
      else
        my_time_stratum = 0x0100;
      // Send time packet
      if (udp_time && (timesocket != -1))
      {
        {
          // Occassionally announce our time
          // T + (our stratum) + (64 bit seconds since 1970) +
          // + (24 bit microseconds)
          // = 1+1+8+3 = 13 bytes
          struct timeval tv;
          gettimeofday(&tv, NULL);

          unsigned char msg_out[1024];
          int offset = 0;
          append_timestamp(msg_out, &offset);

          // Now broadcast on every interface to port 0x5401
          // Oh that's right, UDP sockets don't have an easy way to do that.
          // We could interrogate the OS to ask about all interfaces, but we
          // can instead get away with having a single simple broadcast address
          // supplied as part of the timeserver command line argument.
          struct sockaddr_in addr;
          bzero(&addr, sizeof(addr));
          addr.sin_family = AF_INET;
          addr.sin_port = htons(0x5401);
          int i;
          for (i = 0; time_broadcast_addrs[i]; i++)
          {
            addr.sin_addr.s_addr = inet_addr(time_broadcast_addrs[i]);
            errno = 0;
            sendto(timesocket, msg_out, offset,
                   MSG_DONTROUTE | MSG_DONTWAIT
#ifdef MSG_NOSIGNAL
                       | MSG_NOSIGNAL
#endif
                   ,
                   (const struct sockaddr *)&addr, sizeof(addr));
          }
          // printf("--- Sent %d time announcement packets.\n",i);
        }

        account_time("time server: rx ");

        // Check for time packet
        if (timesocket != -1)
        {
          unsigned char msg[1024];
          int offset = 0;
          int r = recvfrom(timesocket, msg, 1024, 0, NULL, 0);
          if (r == (1 + 1 + 8 + 3))
          {
            // see rxmessages.c for more explanation
            offset++;
            int stratum = msg[offset++];
            struct timeval tv;
            bzero(&tv, sizeof(struct timeval));
            for (int i = 0; i < 8; i++)
              tv.tv_sec |= msg[offset++] << (i * 8);
            for (int i = 0; i < 3; i++)
              tv.tv_usec |= msg[offset++] << (i * 8);
            // ethernet delay is typically 0.1 - 5ms, so assume 5ms
            tv.tv_usec += 5000;
            saw_timestamp("          UDP", stratum, &tv);
          }
        }
      }

      if (httpsocket != -1)
      {
        struct sockaddr cliaddr;
        socklen_t addrlen;
        account_time("HTTP accept()");
        int s = accept(httpsocket, &cliaddr, &addrlen);
        if (s != -1)
        {
          // HTTP request socket
          //	      printf("HTTP Socket connection\n");
          // Process socket
          // XXX This is synchronous to keep things simple,
          // which is part of why we only check every second or so
          // for one new connection.  We also don't allow the request
          // to linger: if it doesn't contain the request almost immediately,
          // we reject it with a timeout error.
          account_time("http_process()");
          http_process(&cliaddr, servald_server, credential, my_sid_hex, s);
        }
      }

      account_time("update_my_message()");

      if ((!monitor_mode) && (radio_ready()))
      {
        update_my_message(serialfd,
                          my_sid, my_sid_hex,
                          LINK_MTU, msg_out,
                          servald_server, credential);

        // Vary next update time by upto 250ms, to prevent radios getting lock-stepped.
        if (message_update_interval_randomness)
          last_message_update_time = gettime_ms() + (random() % message_update_interval_randomness);
        else
          last_message_update_time = gettime_ms();
      }

      account_time("status_dump()");

      // Update the state file to help debug things
      // (but not too often, since it is SLOW on the MR3020s
      //  XXX fix all those linear searches, and it will be fine!)
      if (last_status_time > time(0))
        last_status_time = time(0);
      if (time(0) > last_status_time)
      {
        last_status_time = time(0) + 2;
        status_dump();
      }

      account_time("post_status_dump()");
    }

    account_time("stuck serial reboot check");

    if ((serial_errors > 20) && reboot_when_stuck)
    {
      // If we are unable to write to the serial port repeatedly for a while,
      // we could be facing funny serial port behaviour bugs that we see on the MR3020.
      // In which case, if authorised, ask the MR3020 to reboot
      system("reboot");
    }

    account_time("usleep()");

    usleep(10000);

    account_time("show_progress()");

    if (time(0) > last_summary_time)
    {
      last_summary_time = time(0);
      show_progress(stderr, 0);
    }

    account_time("End of loop");*/
  }
}
