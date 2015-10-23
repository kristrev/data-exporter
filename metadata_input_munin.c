#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "metadata_exporter.h"
#include "metadata_input_munin.h"
#include "backend_event_loop.h"

/* polling interval in seconds */
#define MUNIN_POLLING_INTERVAL 5  

/* TODO
- handle socket disconnect/reconnect 
*/
ssize_t readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                   
    size_t totRead;                  
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }
    buf = buffer;     

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)    
                continue;
            else
                return -1;         

        } else if (numRead == 0) {      
            if (totRead == 0)          
                return 0;
            else                     
                break;

        } else {                       
            if (totRead < n - 1) {  
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}

uint8_t reconnect (struct md_input_munin *mim, const char *address, const char *port) {
    int n = -1;
    char buffer[256] = "\n";
    struct hostent* server;
    struct sockaddr_in serv_addr;

    if ((mim->munin_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      fprintf(stderr, "Error opening socket.\n");
      return RETVAL_FAILURE;
    }
    if ((server = gethostbyname(address)) == NULL) {
      fprintf(stderr, "No such host: %s\n", address);
      return RETVAL_FAILURE;
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(port));
  
    if (connect(mim->munin_socket_fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
      fprintf(stderr, "Could not connect to munin.");
      return RETVAL_FAILURE;
    } 
    
    if ((n = readLine(mim->munin_socket_fd, buffer, 255))<0) {
      fprintf(stderr, "Could not read munin welcome string.");
      close(mim->munin_socket_fd);
      return RETVAL_FAILURE;
    }
    printf("%s", buffer);
    
    return RETVAL_SUCCESS;
}

void json_add_key_value(char* kv, json_object* blob) {
  char *running = kv;
  // a munin string is in the form uptime.value 1.23
  char *key     = strsep(&running, ".");
  strsep(&running, " "); //ignore .value
  char *value   = strsep(&running, "\n");
  

  struct json_object *obj_add = NULL;
  if ((obj_add = json_object_new_string(value))==NULL) 
    return;
  json_object_object_add(blob, key, obj_add);
}

static void md_input_munin_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_munin *mim = ptr;
    int i=0,n;
    uint64_t exp;

    if ((n=read(fd, &exp, sizeof(uint64_t))) < sizeof(uint64_t)) {
        fprintf(stderr, "Munin timer failed.\n");
        return;
    }
     
    setsockopt( mim->munin_socket_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));

    struct json_object *blob = NULL;
    struct timeval tv;

    if (!(blob = json_object_new_object()))
        return;

    const char* const modules[] = {
      "uptime",
      "cpu",
      "memory",
    }; 
    int nmodules = 3;

    for (i=0; i<nmodules; i++) {
      char cmd[256];
      char buffer[256];
      snprintf(cmd, 255, "fetch %s\n", modules[i]);

      if ((n=write(mim->munin_socket_fd, cmd, strlen(cmd)))<0) {
          fprintf(stderr, "Writing to munin failed.\n");
          return;
      }

      struct json_object *obj_mod = NULL;
      if (!(obj_mod = json_object_new_object()))
        break;
      json_object_object_add(blob, modules[i], obj_mod);

      while (1) {
        if ((n=readLine(mim->munin_socket_fd, buffer, 255))<0) {
            fprintf(stderr, "Reading from munin failed.\n");
            return;
        }
        if ((buffer[0] == '#') || (buffer[0] == '.')) 
          break;
        json_add_key_value(buffer, obj_mod);
      }
    }
       
    struct md_munin_event munin_event;
    memset(&munin_event, 0, sizeof(struct md_munin_event));
    gettimeofday(&tv, NULL);

    munin_event.md_type   = META_TYPE_MUNIN;
    munin_event.tstamp    = tv.tv_sec;
    munin_event.json_blob = blob;

    //fprintf(stderr, "JSON dump: %s\n", json_object_to_json_string(blob));
    mde_publish_event_obj(mim->parent, (struct md_event *) &munin_event);
    json_object_put(blob);
}

static uint8_t md_munin_config(struct md_input_munin *mim,
                              const char *address,
                              const char *port)
{
    int timer;
    struct itimerspec delay;

    if (reconnect(mim, address, port) == RETVAL_SUCCESS) {
      if ((timer = timerfd_create(CLOCK_REALTIME, 0)) < 0) {
        fprintf(stderr, "Could not create munin polling timer.");
        return RETVAL_FAILURE;
      }
      
      delay.it_value.tv_sec     = MUNIN_POLLING_INTERVAL;
      delay.it_value.tv_nsec    = 0;
      delay.it_interval.tv_sec  = MUNIN_POLLING_INTERVAL;
      delay.it_interval.tv_nsec = 0;
      if (timerfd_settime(timer, TFD_TIMER_ABSTIME, &delay, NULL) < 0) {
        fprintf(stderr, "Could not initialize munin polling timer.");
        return RETVAL_FAILURE;
      }

      if(!(mim->event_handle = backend_create_epoll_handle(
                               mim, timer, md_input_munin_handle_event)))
        return RETVAL_FAILURE;

      backend_event_loop_update(
          mim->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD, timer, mim->event_handle);

      return RETVAL_SUCCESS;
    } else {
      return RETVAL_FAILURE;
    }
}

static uint8_t md_input_munin_init(void *ptr, int argc, char *argv[])
{
    struct md_input_munin *mim = ptr;
    const char *address = NULL, *port = NULL;
    int c, option_index = 0;

    static struct option munin_options[] = {
        {"munin_address",         required_argument,  0,  0},
        {"munin_port",            required_argument,  0,  0},
        {0,                                      0,  0,  0}};

    while (1) {
        //No permuting of array here as well
        c = getopt_long_only(argc, argv, "--", munin_options, &option_index);

        if (c == -1)
            break;
        else if (c)
            continue;

        if (!strcmp(munin_options[option_index].name, "munin_address"))
            address = optarg;
        else if (!strcmp(munin_options[option_index].name, "munin_port"))
            port = optarg;
    }

    if (address == NULL || port == NULL) {
        fprintf(stderr, "Missing required Munin argument\n");
        return RETVAL_FAILURE;
    }

    return md_munin_config(mim, address, port);
}

static void md_munin_usage()
{
    fprintf(stderr, "Munin input:\n");
    // TODO: --munin-binary (no need to run systemd, nginx or inetd)
    fprintf(stderr, "--munin_address: munin address (r)\n");
    fprintf(stderr, "--munin_port:    munin port    (r)\n");
}

void md_munin_setup(struct md_exporter *mde, struct md_input_munin *mim)
{
    mim->parent = mde;
    mim->init   = md_input_munin_init;
    mim->usage  = md_munin_usage;
}
