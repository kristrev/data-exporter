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
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>

#include "backend_event_loop.h"
#include "metadata_exporter.h"
#include "metadata_input_gps_nsb.h"
#include "lib/minmea.h"
#include "metadata_exporter_log.h"

static void md_input_gps_nsb_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_gps_nsb *mign = ptr;
    char rcv_buf[4096];
    struct md_gps_event gps_event;
    int8_t sentence_id = 0;
    struct minmea_sentence_rmc rmc;
    int32_t retval;

    retval = recv(fd, rcv_buf, sizeof(rcv_buf), 0);

    if (retval <= 0)
        return;

    sentence_id = minmea_sentence_id(rcv_buf, 0);

    if (sentence_id <= 0)
        return;

    memset(&gps_event, 0, sizeof(struct md_gps_event));
    gettimeofday(&gps_event.tstamp_tv, NULL);
    gps_event.md_type = META_TYPE_POS;
    //TODO: HACK until we implement better GPS handling in sqlite writer, NSB
    //only export RMC as far as I can tell
    gps_event.minmea_id = MINMEA_UNKNOWN;

    //We can ignore NMEA checksum
    switch (sentence_id) {
    case MINMEA_SENTENCE_RMC:
        retval = minmea_parse_rmc(&rmc, rcv_buf);

        if (retval && !rmc.valid) {
            retval = 0;
        } else {
            gps_event.time = rmc.time;
            gps_event.latitude = minmea_tocoord(&(rmc.latitude));
            gps_event.longitude = minmea_tocoord(&(rmc.longitude));
            gps_event.speed = minmea_tofloat(&(rmc.speed));
        }
        break;
    default:
        retval = 0;
        return;
    }

    if (!retval)
        return;

    gps_event.sequence = mde_inc_seq(mign->parent);
    mde_publish_event_obj(mign->parent, (struct md_event *) &gps_event);
}

static uint8_t md_input_gps_nsb_config(struct md_input_gps_nsb *mign,
                                       const char *address,
                                       const char *port)
{
    int32_t sockfd = -1;
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(address, port, &hints, &res)) {
        META_PRINT(mign->parent->logfile, "Could not get address info for NSB GPS\n");
        return RETVAL_FAILURE;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd < 0) {
        META_PRINT(mign->parent->logfile, "Failed to create socket for NSB GPS\n");
        return RETVAL_FAILURE;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen)) {
        META_PRINT(mign->parent->logfile, "Failed to bind NSB GPS socket\n");
        return RETVAL_FAILURE;
    }

    if(!(mign->event_handle = backend_create_epoll_handle(mign,
                    sockfd, md_input_gps_nsb_handle_event)))
        return RETVAL_FAILURE;
   
    backend_event_loop_update(mign->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
        sockfd, mign->event_handle);

    return RETVAL_SUCCESS;
}

static uint8_t md_input_gps_nsb_init(void *ptr, int argc, char *argv[])
{
    struct md_input_gps_nsb *mign = ptr;
    const char *address = NULL, *port = NULL;
    int c, option_index = 0;

    static struct option gpsd_options[] = {
        {"nsb_gps_address",         required_argument,  0,  0},
        {"nsb_gps_port",            required_argument,  0,  0},
        {0,                                         0,  0,  0}};

    while (1) {
        //No permuting of array here as well
        c = getopt_long_only(argc, argv, "--", gpsd_options, &option_index);

        if (c == -1)
            break;
        else if (c)
            continue;

        if (!strcmp(gpsd_options[option_index].name, "nsb_gps_address"))
            address = optarg;
        else if (!strcmp(gpsd_options[option_index].name, "nsb_gps_port"))
            port = optarg;
    }

    if (address == NULL || port == NULL) {
        META_PRINT(mign->parent->logfile, "Missing required GPSD argument\n");
        return RETVAL_FAILURE;
    }

    return md_input_gps_nsb_config(mign, address, port);
}

static void md_gps_nsb_usage()
{
    fprintf(stderr, "NSB GPS input:\n");
    fprintf(stderr, "--nsb_gps_address: IP NSB broadcasts GPS to (r)\n");
    fprintf(stderr, "--nsb_gps_port: Port NSB broadcasts GPS to (r)\n");
}

void md_gps_nsb_setup(struct md_exporter *mde, struct md_input_gps_nsb *mign)
{
    mign->parent = mde;
    mign->init = md_input_gps_nsb_init;
    mign->usage = md_gps_nsb_usage;
}
