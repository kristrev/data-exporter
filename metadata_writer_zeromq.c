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
#include <string.h>
#include <zmq.h>
#include JSON_LOC
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>

#include "lib/minmea.h"
#include "metadata_exporter.h"
#include "metadata_writer_zeromq.h"
#include "system_helpers.h"
#include "metadata_utils.h"

static json_object* md_zeromq_create_json_gps(struct md_writer_zeromq *mwz,
                                              struct md_gps_event *mge)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!(obj_add = json_object_new_int(mge->sequence))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "seq", obj_add);

    if (!(obj_add = json_object_new_int64(mge->tstamp))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "tstamp", obj_add);

    if (!(obj_add = json_object_new_double(mge->latitude))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "lat", obj_add);

    if (!(obj_add = json_object_new_double(mge->longitude))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "lng", obj_add);

    if (mge->speed) {
        obj_add = json_object_new_double(mge->speed);

        if (obj_add == NULL) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, "speed", obj_add);
    }

    if (mge->altitude) {
        obj_add = json_object_new_double(mge->altitude);

        if (obj_add == NULL) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, "altitude", obj_add);
    }

    if (mge->satellites_tracked) {
        obj_add = json_object_new_int(mge->satellites_tracked);

        if (obj_add == NULL) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, "num_sat", obj_add);
    }

    return obj;
}

static json_object* md_zeromq_create_json_gps_raw(struct md_writer_zeromq *mwz,
                                                  struct md_gps_event *mge)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!(obj_add = json_object_new_int(mde_inc_seq(mwz->parent)))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "seq", obj_add);

    if (!(obj_add = json_object_new_int64(mge->tstamp))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "tstamp", obj_add);

    if (!(obj_add = json_object_new_string(mge->nmea_raw))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "nmea_raw", obj_add);

    
    return obj;
}

static void md_zeromq_handle_gps(struct md_writer_zeromq *mwz,
                                 struct md_gps_event *mge)
{
    char topic[8192];
    struct json_object *gps_obj = md_zeromq_create_json_gps(mwz, mge);
    int retval;

    if (gps_obj == NULL) {
        fprintf(stderr, "Failed to create GPS ZMQ JSON\n");
        return;
    }

    snprintf(topic, sizeof(topic), "MONROE.GPS|%s", json_object_to_json_string_ext(gps_obj, JSON_C_TO_STRING_PLAIN));
   
    //TODO: Error handling
    zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    json_object_put(gps_obj);

    if (!mge->nmea_raw)
        return;

    gps_obj = md_zeromq_create_json_gps_raw(mwz, mge);

    if (gps_obj == NULL) {
        fprintf(stderr, "Failed to create RAW GPS JSON\n");
        return;
    }

    retval = snprintf(topic, sizeof(topic), "MONROE.GPS.RAW|%s",
            json_object_to_json_string_ext(gps_obj, JSON_C_TO_STRING_PLAIN));

    if (retval >= sizeof(topic)) {
        json_object_put(gps_obj);
        return;
    }

    zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    json_object_put(gps_obj);
}

static json_object* md_zeromq_create_json_modem_default(struct md_writer_zeromq *mwz,
                                                        struct md_conn_event *mce)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (!(obj_add = json_object_new_int(mce->sequence))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "seq", obj_add);

    if (!(obj_add = json_object_new_int64(mce->tstamp))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "tstamp", obj_add);

    if (!(obj_add = json_object_new_string(mce->interface_id))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "interface_id", obj_add);

    if (!(obj_add = json_object_new_int(mce->network_provider))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "operator", obj_add);

    return obj;
}

static void md_zeromq_handle_conn(struct md_writer_zeromq *mwz,
                                   struct md_conn_event *mce)
{
    struct json_object *json_obj, *obj_add;
    char event_str_cpy[EVENT_STR_LEN];
    size_t event_str_len;
    uint8_t mode;
    char topic[8192];
    int retval;

    if ((mce->event_param != CONN_EVENT_MODE_CHANGE &&
         mce->event_param != CONN_EVENT_META_UPDATE) ||
         mce->interface_type != INTERFACE_MODEM)
        return;

    if (mce->event_param == CONN_EVENT_MODE_CHANGE) {
        mode = mce->event_value;
    } else {
        event_str_len = strlen(mce->event_value_str);

        if (event_str_len >= EVENT_STR_LEN) {
            fprintf(stderr, "Event string too long\n");
            return;
        }

        memcpy(event_str_cpy, mce->event_value_str, event_str_len);
        event_str_cpy[event_str_len] = '\0';
        mode = metadata_utils_get_csv_pos(event_str_cpy, 2);
    }
    
    json_obj = md_zeromq_create_json_modem_default(mwz, mce);

    if (!(obj_add = json_object_new_int(mode))) {
        json_object_put(json_obj);
        return;
    }
    json_object_object_add(json_obj, "mode", obj_add);

    if (mce->event_param == CONN_EVENT_META_UPDATE &&
        mce->signal_strength != -127) {
        obj_add = json_object_new_int(mce->signal_strength);

        if (!obj_add) {
            json_object_put(json_obj);
            return;
        } else {
            json_object_object_add(json_obj, "signal_strength", obj_add);
        }
    }

    if (mce->event_param == CONN_EVENT_META_UPDATE)
        retval = snprintf(topic, sizeof(topic), "MONROE.META.DEVICE.MODEM.UPDATE|%s",
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
    else
        retval = snprintf(topic, sizeof(topic), "MONROE.META.DEVICE.MODEM.MODE_CHANGE|%s",
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));

    if (retval >= sizeof(topic)) {
        json_object_put(json_obj);
        return;
    }

    zmq_send(mwz->zmq_publisher, topic, strlen(topic), 0);
    json_object_put(json_obj);
}

static void md_zeromq_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_zeromq *mwz = (struct md_writer_zeromq*) writer;

    switch (event->md_type) {
    case META_TYPE_POS:
        md_zeromq_handle_gps(mwz, (struct md_gps_event*) event);
        break;
    case META_TYPE_CONNECTION:
        md_zeromq_handle_conn(mwz, (struct md_conn_event*) event);
    default:
        break;
    }
}

static uint8_t md_zeromq_config(struct md_writer_zeromq *mwz,
                                const char *address,
                                uint16_t port)
{
    //INET6_ADDRSTRLEN is 46 (max length of ipv6 + trailing 0), 5 is port, 6 is
    //protocol (we right now only support TCP)
    char zmq_addr[INET6_ADDRSTRLEN + 5 + 6];
    int32_t retval;

    snprintf(zmq_addr, sizeof(zmq_addr), "tcp://%s:%d", address, port);

    if ((mwz->zmq_context = zmq_ctx_new()) == NULL)
        return RETVAL_FAILURE;

    if ((mwz->zmq_publisher = zmq_socket(mwz->zmq_context, ZMQ_PUB)) == NULL)
        return RETVAL_FAILURE;

    if ((retval = zmq_bind(mwz->zmq_publisher, zmq_addr)) != 0) {
        printf("%d %s\n", errno, zmq_strerror(errno));
        return RETVAL_FAILURE;
    }

    printf("ZeroMQ init done\n");

    return RETVAL_SUCCESS;
}

static int32_t md_zeromq_init(void *ptr, int argc, char *argv[])
{
    struct md_writer_zeromq *mwz = ptr;
    const char *address = NULL;
    uint16_t port = NULL;
    int c, option_index = 0;

    static struct option zmq_options[] = {
        {"zmq_address",         required_argument,  0,  0},
        {"zmq_port",            required_argument,  0,  0},
        {0,                                     0,  0,  0}};

    while (1) {
        //No permuting of array here as well
        c = getopt_long_only(argc, argv, "--", zmq_options, &option_index);


        if (c == -1)
            break;
        else if (c)
            continue;
        
        if (!strcmp(zmq_options[option_index].name, "zmq_address"))
            address = optarg;
        else if (!strcmp(zmq_options[option_index].name, "zmq_port"))
            port = (uint16_t) atoi(optarg);
    }

    if (address == NULL || port == 0) {
        fprintf(stderr, "Missing required ZeroMQ argument\n");
        return RETVAL_FAILURE;
    }

    if (system_helpers_check_address(address)) {
        fprintf(stderr, "Error in ZeroMQ address\n");
        return RETVAL_FAILURE;
    }

    return md_zeromq_config(mwz, address, port);
}

static void md_zeromq_usage()
{
    fprintf(stderr, "ZeroMQ writer:\n");
    fprintf(stderr, "--zmq_address: address used by publisher (r)\n");
    fprintf(stderr, "--zmq_port: port used by publisher (r)\n");
}

void md_zeromq_setup(struct md_exporter *mde, struct md_writer_zeromq* mwz) {
    mwz->parent = mde;
    mwz->usage = md_zeromq_usage;
    mwz->init = md_zeromq_init;
    mwz->handle = md_zeromq_handle;
}

