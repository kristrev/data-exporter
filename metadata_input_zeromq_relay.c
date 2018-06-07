/* Copyright (c) 2018, Karlstad Universitet, Jonas Karlsson <jonas.karlsson@kau.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libmnl/libmnl.h>
#include JSON_LOC
#include <getopt.h>
#include <sys/time.h>
#include <zmq.h>

#include "metadata_exporter.h"
#include "metadata_input_nl_zmq_common.h"
#include "metadata_input_zeromq_relay.h"
#include "backend_event_loop.h"

#include "lib/minmea.h"
#include "metadata_exporter_log.h"

static void md_input_zeromq_relay_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_zeromq_relay *miz = ptr;
    int zmq_events = 0;
    size_t events_len = sizeof(zmq_events);
    json_object *zmqh_obj = NULL;
    const char *json_msg;

    zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);

    while (zmq_events & ZMQ_POLLIN)
    {
        char buf[2048] = {0};
        zmq_recv(miz->zmq_socket, buf, 2048, 0);

        json_msg = strchr(buf, '{');
        // Sanity checks 
        // Do we even have a json object 
        if (json_msg == NULL)
        {
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        // Is the json object valid ?
        zmqh_obj = json_tokener_parse(json_msg);
        if (!zmqh_obj) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Received invalid JSON object on ZMQ socket\n");
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }
        //TODO: Check so we also have a topic

        META_PRINT(miz->parent->logfile, "Got JSON %s\n", json_object_to_json_string(zmqh_obj));
        json_object_put(zmqh_obj);
        
        //Yay we have a valid object lets store that and publish it.
        
        //Create a zeromq event
        memset(miz->mse, 0, sizeof(struct md_zeromq_event));
        miz->mse->md_type = META_TYPE_ZEROMQ;
        miz->mse->msg = buf;
        mde_publish_event_obj(miz->parent, (struct md_event*) miz->mse);

        zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
    }
}

static uint8_t md_input_zeromq_relay_config(struct md_input_zeromq_relay *miz)
{
    int zmq_fd = -1;
    size_t len = 0;

    // Connect to ZMQ publisher(s)
    // first version connects to one publisher, next version should accept multiple publishers
    miz->zmq_ctx = zmq_ctx_new();
    if (miz->zmq_ctx == NULL) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't create ZMQ context\n");
        return RETVAL_FAILURE;
    }

    miz->zmq_socket = zmq_socket(miz->zmq_ctx, ZMQ_SUB);
    if (miz->zmq_socket == NULL) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't create ZMQ socket\n");
        return RETVAL_FAILURE;
    }

    //Connect to user defined publiser $URL 
    if (zmq_connect(miz->zmq_socket, miz->zmq_pub_url) == -1)
    {
        char buf[1024] = {0};
        snprintf(buf. sizeof(buf), "Can't connect to %s ZMQ publisher\n",miz->zmq_pub_url )
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, buf);
        return RETVAL_FAILURE;
    }

    // subscribe to all topics (of this publisher)
    const char *topic = "";
    zmq_setsockopt(miz->zmq_socket, ZMQ_SUBSCRIBE, topic, strlen(topic));
    
    len = sizeof(zmq_fd);
    if (zmq_getsockopt(miz->zmq_socket, ZMQ_FD, &zmq_fd, &len) == -1) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't get ZMQ file descriptor\n");
        return RETVAL_FAILURE;
    }

    if(!(miz->event_handle = backend_create_epoll_handle(miz,
                    zmq_fd, md_input_zeromq_relay_handle_event)))
        return RETVAL_FAILURE;

    backend_event_loop_update(miz->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
        zmq_fd, miz->event_handle);

    return RETVAL_SUCCESS;
}

static uint8_t md_input_zeromq_relay_init(void *ptr, json_object* config)
{
    struct md_input_zeromq_relay *miz = ptr;
    char url[256] = {0};
    
    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq_input_relay", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "url"))
                miz->zmq_pub_url = strncpy(url,val, sizeof(url));
        }
    }


    if (!miz->zmq_pub_url) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "At least one publisher must be present\n");
        return RETVAL_FAILURE;
    }

    return md_input_zeromq_relay_config(miz);
}

void md_zeromq_relay_usage()
{
    fprintf(stderr, "\"zmq_input\": {\t\tZeroMQ input (at least one url must be present)\n");
    fprintf(stderr, "  \"url\":\t\tListen to ZeroMQ events on this URL\n");
    fprintf(stderr, "},\n");
}

void md_zeromq_relay_setup(struct md_exporter *mde, struct md_input_zeromq_relay *miz)
{
    miz->parent = mde;
    miz->init = md_input_zeromq_relay_init;
}
