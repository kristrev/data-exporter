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

static int hashCode(struct table *t,int key){
    if(key<0)
        return -(key%t->size);
    return key%t->size;
}

void insert(struct table *t,int key, struct zmq_connection *val){
    int pos = hashCode(t,key);
    struct node *list = t->list[pos];
    struct node *newNode = (struct node*)malloc(sizeof(struct node));
    struct node *temp = list;
    while(temp){
        if(temp->key==key){
            temp->val = val;
            return;
        }
        temp = temp->next;
    }
    newNode->key = key;
    newNode->val = val;
    newNode->next = list;
    t->list[pos] = newNode;
}

struct table *createTable(int size){
    struct table *t = (struct table*)malloc(sizeof(struct table));
    t->size = size;
    t->list = (struct node**)malloc(sizeof(struct node*)*size);
    int i;
    for(i=0;i<size;i++)
        t->list[i] = NULL;
    return t;
};

static struct zmq_connection* lookup(struct table *t,int key){
    int pos = hashCode(t,key);
    struct node *list = t->list[pos];
    struct node *temp = list;
    while(temp){
        if(temp->key==key){
            return temp->val;
        }
        temp = temp->next;
    }
    return NULL;
}

static void md_input_zeromq_relay_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_zeromq_relay *miz = ptr;
    int zmq_events = 0;
    size_t events_len = sizeof(zmq_events);
    json_object *zmqh_obj = NULL;
    const char *json_msg;
    struct zmq_connection *zmq_con = lookup(miz->zmq_connections, fd);
    void *zmq_socket;

    if (!zmq_con) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Could not lookup ZMQ connection\n");
            return;
    }
    
    zmq_socket = zmq_con->zmq_socket;
    if (!zmq_socket) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Not a valid ZMQ socket\n");
            return;
    }
   
    zmq_getsockopt(zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);

    while (zmq_events & ZMQ_POLLIN)
    {
        char buf[2048] = {0};
        zmq_recv(zmq_socket, buf, 2048, 0);

        json_msg = strchr(buf, '{');
        // Sanity checks 
        // Do we even have a json object 
        if (json_msg == NULL)
        {
            zmq_getsockopt(zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        // Is the json object valid ?
        zmqh_obj = json_tokener_parse(json_msg);
        if (!zmqh_obj) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Received invalid JSON object on ZMQ socket\n");
            zmq_getsockopt(zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }
        //TODO: Check so we also have a topic

        META_PRINT(miz->parent->logfile, "Got JSON %s\n", json_object_to_json_string(zmqh_obj));
        json_object_put(zmqh_obj);
        
        //Yay we have a valid object lets store that and publish it.

        //Create a zeromq event
        //This works all work as the consuming function(s) are called sync, could mde_publish_event_obj ever be asynch?
        memset(miz->mse, 0, sizeof(struct md_zeromq_event));
        miz->mse->md_type = META_TYPE_ZEROMQ;
        miz->mse->msg = buf;  //this will fail if next line is async
        mde_publish_event_obj(miz->parent, (struct md_event*) miz->mse);

        zmq_getsockopt(zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
    }
}

static uint8_t md_input_zeromq_relay_config(struct md_input_zeromq_relay *miz)
{
    int zmq_fd = -1;
    size_t len = 0;
    struct zmq_connection *zmq_con; 
    miz->zmq_connections = createTable(miz->nr_of_connections);  //len of miz->urls

    //We only handle one message at a time even if we listen to multiple publishers
    miz->mse = calloc(1, sizeof(struct md_zeromq_event));
    if (miz->mse == NULL)
        return RETVAL_FAILURE;

    // Connect to ZMQ publisher(s)
    for (int i = 0; i < miz->nr_of_connections; i++) {
        zmq_con = calloc(1, sizeof(struct zmq_connection));
        zmq_con->zmq_ctx = zmq_ctx_new();
        if (zmq_con->zmq_ctx == NULL) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't create ZMQ context\n");
            return RETVAL_FAILURE;
        }

        zmq_con->zmq_socket = zmq_socket(zmq_con->zmq_ctx, ZMQ_SUB);
        if (zmq_con->zmq_socket == NULL) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't create ZMQ socket\n");
            return RETVAL_FAILURE;
        }

        //Connect to user defined publiser $URL 
        if (zmq_connect(zmq_con->zmq_socket, miz->urls[i]) == -1)
        {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't connect to %s ZMQ publisher\n", miz->urls[i]);
            return RETVAL_FAILURE;
        }

        // subscribe to all topics (of this publisher)
        const char *topic = "";
        zmq_setsockopt(zmq_con->zmq_socket, ZMQ_SUBSCRIBE, topic, strlen(topic));
        
        len = sizeof(zmq_fd);
        if (zmq_getsockopt(zmq_con->zmq_socket, ZMQ_FD, &zmq_fd, &len) == -1) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't get ZMQ file descriptor\n");
            return RETVAL_FAILURE;
        }

        if(!(miz->event_handle = backend_create_epoll_handle(miz,
                        zmq_fd, md_input_zeromq_relay_handle_event)))
            return RETVAL_FAILURE;

        backend_event_loop_update(miz->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
            zmq_fd, miz->event_handle);
        insert(miz->zmq_connections,zmq_fd,zmq_con);
    }
    
    return RETVAL_SUCCESS;
}

static uint8_t md_input_zeromq_relay_init(void *ptr, json_object* config)
{
    struct md_input_zeromq_relay *miz = ptr;
    miz->nr_of_connections = 0; 
    
    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq_input_relay", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
	        if (!strcmp(key, "urls")) {
                miz->nr_of_connections = json_object_array_length(val);
                miz->urls= calloc(miz->nr_of_connections, sizeof(char*));
                for (int i=0; i< miz->nr_of_connections; i++) {
                    struct json_object* json_url = json_object_array_get_idx(val,i);
                    int url_len = json_object_get_string_len(json_url) + 1;
                    const char * url = json_object_get_string(json_url);
                    miz->urls[i] = calloc(url_len, sizeof(char));
            		snprintf((char *)miz->urls[i],url_len, "%s", url);
                }
	        }
        }
    }


    if (miz->nr_of_connections <= 0) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "At least one publisher must be present\n");
        return RETVAL_FAILURE;
    }

    return md_input_zeromq_relay_config(miz);
}

void md_zeromq_relay_usage()
{
    fprintf(stderr, "\"zmq_input_relay\": {\tZeroMQ input (at least one url must be present)\n");
    fprintf(stderr, "  \"urls\":\t\tArray of ZeroMQ URLs to listen to, \
eg. [\"tcp://127.0.0.1:10001\", \"tcp://127.0.0.1:10002\"] \n");
    fprintf(stderr, "},\n");
}

void md_zeromq_relay_setup(struct md_exporter *mde, struct md_input_zeromq_relay *miz)
{
    miz->parent = mde;
    miz->init = md_input_zeromq_relay_init;
}
