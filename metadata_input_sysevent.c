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
#include <zmq.h>

#include "metadata_exporter.h"
#include "metadata_input_sysevent.h"
#include "backend_event_loop.h"

/* TODO
- clean header list 
*/

static uint8_t md_sysevent_reconnect(struct md_input_sysevent *mis)
{
    void *context = zmq_ctx_new();
    mis->responder = zmq_socket(context, ZMQ_REP);

    int rc = zmq_bind(mis->responder, "ipc:///tmp/sysevent");
    if (rc != 0) return RETVAL_FAILURE;
    
    size_t len=-1;
    mis->zmq_fd = 0;
    if (zmq_getsockopt(mis->responder, ZMQ_FD, &(mis->zmq_fd), &len) != 0) { 
        fprintf(stderr, "zmq_getsockopt failed to get file descriptor.\n");
        return RETVAL_FAILURE;
    }
    return RETVAL_SUCCESS;
}

static void md_input_sysevent_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_sysevent *mis = ptr;
    char buffer[8192] = {0};

    int zevents = 0;
    size_t zevents_len = sizeof(zevents);    
    zmq_getsockopt(mis->responder, ZMQ_EVENTS, &zevents, &zevents_len);

    do {
        if (zevents & ZMQ_POLLIN) {
            int nbytes = zmq_recv(mis->responder, &buffer, 8192, ZMQ_NOBLOCK);
            if (nbytes<sizeof(buffer)) {
                           
                struct md_sysevent sys_event = {0};
                struct timeval tv;
                gettimeofday(&tv, NULL);

                sys_event.md_type   = META_TYPE_SYSEVENT;
                sys_event.tstamp    = tv.tv_sec;
                sys_event.sequence  = mde_inc_seq(mis->parent);

                json_tokener *tok = json_tokener_new();
                json_object *parsed = json_tokener_parse_ex(tok, buffer, strlen(buffer));
                if (parsed != NULL) {
                    zmq_send(mis->responder, "Takk\n", 5, 0);
                    sys_event.json_blob = parsed;
                    mde_publish_event_obj(mis->parent, (struct md_event *) &sys_event);
                    json_object_put(parsed);
                } else {
                    enum json_tokener_error jerr = json_tokener_get_error(tok);
                    fprintf(stderr, "%s (Message was %s)\n", json_tokener_error_desc(jerr), buffer);
                    zmq_send(mis->responder, "That wasn't JSON.\n", 18, 0);
                }
                json_tokener_free(tok);

            }
        }
        zmq_getsockopt(mis->responder, ZMQ_EVENTS, &zevents, &zevents_len);
    } while (zevents & ZMQ_POLLIN);
}

static uint8_t md_sysevent_config(struct md_input_sysevent *mis)
{
    if (md_sysevent_reconnect(mis) == RETVAL_SUCCESS) {
      if(!(mis->event_handle = backend_create_epoll_handle(
                               mis, mis->zmq_fd , md_input_sysevent_handle_event)))
        return RETVAL_FAILURE;

      backend_event_loop_update(
          mis->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD, mis->zmq_fd, mis->event_handle);
        
      return RETVAL_SUCCESS;
    } 
    return RETVAL_FAILURE;
}

static uint8_t md_input_sysevent_init(void *ptr, int argc, char *argv[])
{
    struct md_input_sysevent *mis = ptr;
    return md_sysevent_config(mis);
}

static void md_input_sysevent_usage()
{
}

static void md_input_sysevent_destroy()
{
}

void md_sysevent_setup(struct md_exporter *mde, struct md_input_sysevent *mis)
{
    mis->parent  = mde;
    mis->init    = md_input_sysevent_init;
    mis->destroy = md_input_sysevent_destroy;
    mis->usage   = md_input_sysevent_usage;
}
