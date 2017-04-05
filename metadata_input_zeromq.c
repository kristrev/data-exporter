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

#include "metadata_exporter.h"
#include "metadata_input_zeromq.h"
#include "backend_event_loop.h"

#include "lib/minmea.h"
#include "metadata_exporter_log.h"

static uint8_t md_input_zeromq_config(struct md_input_zeromq *miz)
{
    return RETVAL_SUCCESS;
}

static uint8_t md_input_zeromq_init(void *ptr, json_object* config)
{
    struct md_input_zeromq *miz = ptr;
    uint32_t md_nl_mask = 0;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq_input", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "conn")) 
                md_nl_mask |= META_TYPE_CONNECTION;
            else if (!strcmp(key, "pos")) 
                md_nl_mask |= META_TYPE_POS;
            else if (!strcmp(key, "iface")) 
                md_nl_mask |= META_TYPE_INTERFACE;
            else if (!strcmp(key, "radio"))
                md_nl_mask |= META_TYPE_RADIO;
        }
    }

    if (!md_nl_mask) {
        META_PRINT_SYSLOG(miz->parent, LOG_ERR, "At least one netlink event type must be present\n");
        return RETVAL_FAILURE;
    }

    miz->md_nl_mask = md_nl_mask;

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
