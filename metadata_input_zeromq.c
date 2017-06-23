/* Copyright (c) 2015, Celerway, Kristian Evensen <kristrev@celerway.com>
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
#include "metadata_input_zeromq.h"
#include "backend_event_loop.h"

#include "lib/minmea.h"
#include "metadata_exporter_log.h"

static void md_input_zeromq_handle_iface_event(struct md_input_zeromq *miz,
        struct json_object *obj)
{
    init_iface_event(miz->mie);

    if (parse_iface_event(obj, miz->mie, miz->parent) == RETVAL_FAILURE)
        return;

    mde_publish_event_obj(miz->parent, (struct md_event*) miz->mie);
}

static void md_input_zeromq_handle_conn_event(struct md_input_zeromq *miz,
        struct json_object *obj)
{
    uint8_t retval = 0;

    init_conn_event(miz->mce);
    retval = parse_conn_info(obj, miz->mce, miz->parent);

    if (retval == RETVAL_FAILURE)
        return;

    mde_publish_event_obj(miz->parent, (struct md_event*) miz->mce);
}

static int subscribe_for_topic(const char* topic, struct md_input_zeromq *miz)
{
    size_t len = strlen(topic);
    return zmq_setsockopt(miz->zmq_socket, ZMQ_SUBSCRIBE, topic, len);
}

static void md_input_zeromq_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_zeromq *miz = ptr;
    int zmq_events = 0;
    size_t events_len = sizeof(zmq_events);
    json_object *json_event = NULL, *zmqh_obj = NULL;
    const char *json_msg;
    uint8_t event_type = 0;

    zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);

    while (zmq_events & ZMQ_POLLIN)
    {
        char buf[2048] = {0};
        zmq_recv(miz->zmq_socket, buf, 2048, 0);

        META_PRINT_SYSLOG(miz->parent, LOG_DEBUG, "Received message: %s\n", buf);

        json_msg = strchr(buf, '{');
        if (json_msg == NULL)
        {
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        zmqh_obj = json_tokener_parse(json_msg);
        if (!zmqh_obj) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Received invalid JSON object on ZMQ socket\n");
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        //We are inserting version and sequence number. Version is so that the
        //application handling this data knows if it supports the format or not.
        //Sequence is so that we can see the order in which events arrived at the
        //metadata exporter, making it easier to correlate events between
        //applications. The different applications publishing data might also insert
        //their own sequence number
        if (add_json_key_value("md_seq", mde_inc_seq(miz->parent), zmqh_obj) ||
            add_json_key_value("md_ver", MDE_VERSION, zmqh_obj)) {
            json_object_put(zmqh_obj);
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        if (!json_object_object_get_ex(zmqh_obj, "event_type", &json_event)) {
            META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Missing event type\n");
            json_object_put(zmqh_obj);
            zmq_getsockopt(miz->zmq_socket, ZMQ_EVENTS, &zmq_events, &events_len);
            continue;
        }

        event_type = (uint8_t) json_object_get_int(json_event);

        if (!(event_type & miz->md_zmq_mask)) {
            json_object_put(zmqh_obj);
            return;
        }

        META_PRINT(miz->parent->logfile, "Got JSON %s\n", json_object_to_json_string(zmqh_obj));

        switch (event_type) {
            case META_TYPE_INTERFACE:
                md_input_zeromq_handle_iface_event(miz, zmqh_obj);
                break;
            case META_TYPE_CONNECTION:
                md_input_zeromq_handle_conn_event(miz, zmqh_obj);
                break;
            case META_TYPE_POS:
                //md_input_netlink_handle_gps_event(miz, zmqh_obj);
                break;
            case META_TYPE_RADIO:
                //md_input_netlink_handle_radio_event(miz, zmqh_obj);
                break;
            case META_TYPE_SYSTEM:
                //md_input_netlink_handle_system_event(miz, zmqh_obj);
                break;
            default:
                META_PRINT(miz->parent->logfile, "Unknown event type\n");
                break;
    }

    json_object_put(zmqh_obj);
    }
}

static uint8_t md_input_zeromq_config(struct md_input_zeromq *miz)
{
    int zmq_fd = -1;
    size_t len = 0;

    // Create ZMQ publisher
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

    if (((miz->md_zmq_mask & META_TYPE_INTERFACE) ||
        (miz->md_zmq_mask & META_TYPE_POS) ||
        (miz->md_zmq_mask & META_TYPE_RADIO)) &&
        zmq_connect(miz->zmq_socket, "ipc:///tmp/nl_pub") == -1)
    {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't connect to NL ZMQ publisher\n");
        return RETVAL_FAILURE;
    }

    if ((miz->md_zmq_mask & META_TYPE_CONNECTION) &&
        zmq_connect(miz->zmq_socket, "ipc:///tmp/dlb_pub") == -1)
    {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't connect to DLB ZMQ publisher\n");
        return RETVAL_FAILURE;
    }

    if (miz->md_zmq_mask & META_TYPE_INTERFACE)
        subscribe_for_topic(ZMQ_NL_INTERFACE_TOPIC, miz);

    if (miz->md_zmq_mask & META_TYPE_RADIO)
        subscribe_for_topic(ZMQ_NL_RADIOEVENT_TOPIC, miz);

    if (miz->md_zmq_mask & META_TYPE_POS)
        subscribe_for_topic(ZMQ_NL_GPS_TOPIC, miz);

    if (miz->md_zmq_mask & META_TYPE_CONNECTION) {
        subscribe_for_topic(ZMQ_DLB_METADATA_TOPIC, miz);
        subscribe_for_topic(ZMQ_DLB_DATAUSAGE_TOPIC, miz);
    }

    len = sizeof(zmq_fd);
    if (zmq_getsockopt(miz->zmq_socket, ZMQ_FD, &zmq_fd, &len) == -1) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "Can't get ZMQ file descriptor\n");
        return RETVAL_FAILURE;
    }

    if(!(miz->event_handle = backend_create_epoll_handle(miz,
                    zmq_fd, md_input_zeromq_handle_event)))
        return RETVAL_FAILURE;

    backend_event_loop_update(miz->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
        zmq_fd, miz->event_handle);

    return RETVAL_SUCCESS;
}

static uint8_t md_input_zeromq_init(void *ptr, json_object* config)
{
    struct md_input_zeromq *miz = ptr;
    miz->md_zmq_mask = 0;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq_input", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "conn")) 
                miz->md_zmq_mask |= META_TYPE_CONNECTION;
            else if (!strcmp(key, "pos")) 
                miz->md_zmq_mask |= META_TYPE_POS;
            else if (!strcmp(key, "iface")) 
                miz->md_zmq_mask |= META_TYPE_INTERFACE;
            else if (!strcmp(key, "radio"))
                miz->md_zmq_mask |= META_TYPE_RADIO;
        }
    }

    if (!miz->md_zmq_mask) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "At least one netlink event type must be present\n");
        return RETVAL_FAILURE;
    }

    return md_input_zeromq_config(miz);
}

void md_zeromq_input_usage()
{
    fprintf(stderr, "\"zmq_input\": {\t\tZeroMQ input (at least one event type must be present)\n");
    fprintf(stderr, "  \"conn\":\t\tReceive netlink connection events\n");
    fprintf(stderr, "  \"pos\":\t\tReceive netlink position events\n");
    fprintf(stderr, "  \"iface\":\t\tReceive netlink interface events\n");
    fprintf(stderr, "  \"radio\":\t\tReceive netlink radio events (QXDM + neigh. cells)\n");
    fprintf(stderr, "},\n");
}

void md_zeromq_input_setup(struct md_exporter *mde, struct md_input_zeromq *miz)
{
    miz->parent = mde;
    miz->init = md_input_zeromq_init;
}