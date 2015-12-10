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
#include <gps.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "metadata_exporter.h"
#include "metadata_input_gpsd.h"
#include "backend_event_loop.h"
#include "metadata_exporter_log.h"

static uint8_t md_input_gpsd_connect(struct md_input_gpsd *mig);

static void md_input_gpsd_handle_connect_timeout(void *ptr)
{
    struct md_input_gpsd *mig = ptr;

    META_PRINT(mig->parent->logfile, "Will attempt GPSD reconnect\n");

    if (md_input_gpsd_connect(mig))
        return;

    META_PRINT(mig->parent->logfile, "GPSD reconnect succesful\n");

    //Stop timer if connect is successful
    //TODO: Currently, the event loop removes timers and the adds the timer back
    //to the list again if interval is set. This is not really ideal behavior,
    //it makes it a bit tricky to properly clean-up timeouts. However, no time
    //to look into that now, so just set intvl to 0 to get desired behavior
    mig->connect_timeout_handle->intvl = 0;
}

static void md_input_gpsd_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_gpsd *mig = ptr;
    struct md_gps_event gps_event;

    if (gps_read(&(mig->gps_data)) <= 0) {
        META_PRINT(mig->parent->logfile, "Failed to read data from GPS\n");
        gps_close(&(mig->gps_data));

        //Use a timer to try to reconnect to gpsd, since it might take some time
        //before it is restarted. No need to use the helper function and set a
        //timestamp, we want the timeout be triggered on next iteration of loop
        mig->connect_timeout_handle->intvl = 5000;
        backend_insert_timeout(mig->parent->event_loop,
                mig->connect_timeout_handle);
        return;
    }


    if (!(mig->gps_data.set & MODE_SET) ||
        mig->gps_data.fix.mode < 2)
        return;

    memset(&gps_event, 0, sizeof(struct md_gps_event));
    gettimeofday(&gps_event.tstamp_tv, NULL);
    gps_event.md_type = META_TYPE_POS;
    gps_event.minmea_id = MINMEA_UNKNOWN;
    gps_event.sequence = mde_inc_seq(mig->parent);
    gps_event.latitude = mig->gps_data.fix.latitude;
    gps_event.longitude = mig->gps_data.fix.longitude;
    gps_event.satellites_tracked = mig->gps_data.satellites_visible;

    if (mig->gps_data.set & ALTITUDE_SET)
        gps_event.altitude = mig->gps_data.fix.altitude;

    if (mig->gps_data.set & SPEED_SET)
        gps_event.speed = mig->gps_data.fix.speed;

    mde_publish_event_obj(mig->parent, (struct md_event *) &gps_event);
}

static uint8_t md_input_gpsd_connect(struct md_input_gpsd *mig)
{
    memset(&(mig->gps_data), 0, sizeof(struct gps_data_t));

    if (gps_open(mig->gpsd_addr, mig->gpsd_port, &(mig->gps_data))) {
        META_PRINT(mig->parent->logfile, "GPS error: %s\n", gps_errstr(errno));
        return RETVAL_FAILURE;
    }

    if (gps_stream(&(mig->gps_data), WATCH_ENABLE | WATCH_JSON, NULL)) {
        META_PRINT(mig->parent->logfile, "GPS error: %s\n", gps_errstr(errno));
        return RETVAL_FAILURE;
    }

    backend_event_loop_update(mig->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
        mig->gps_data.gps_fd, mig->event_handle);

    return RETVAL_SUCCESS;
}

static uint8_t md_gpsd_config(struct md_input_gpsd *mig,
                              const char *address,
                              const char *port)
{
    if(!(mig->event_handle = backend_create_epoll_handle(mig,
                    mig->gps_data.gps_fd, md_input_gpsd_handle_event)))
        return RETVAL_FAILURE;
    
    if(!(mig->connect_timeout_handle = backend_event_loop_create_timeout(0,
                    md_input_gpsd_handle_connect_timeout, mig, 0)))
        return RETVAL_FAILURE;
    
    //Cache address/port in cache gpsd fails while running
    mig->gpsd_addr = address;
    mig->gpsd_port = port;

    return md_input_gpsd_connect(mig);
}

static uint8_t md_input_gpsd_init(void *ptr, int argc, char *argv[])
{
    struct md_input_gpsd *mig = ptr;
    const char *address = NULL, *port = NULL;
    int c, option_index = 0;

    static struct option gpsd_options[] = {
        {"gpsd_address",         required_argument,  0,  0},
        {"gpsd_port",            required_argument,  0,  0},
        {0,                                      0,  0,  0}};

    while (1) {
        //No permuting of array here as well
        c = getopt_long_only(argc, argv, "--", gpsd_options, &option_index);

        if (c == -1)
            break;
        else if (c)
            continue;

        if (!strcmp(gpsd_options[option_index].name, "gpsd_address"))
            address = optarg;
        else if (!strcmp(gpsd_options[option_index].name, "gpsd_port"))
            port = optarg;
    }

    if (address == NULL || port == NULL) {
        META_PRINT(mig->parent->logfile, "Missing required GPSD argument\n");
        return RETVAL_FAILURE;
    }

    return md_gpsd_config(mig, address, port);
}

static void md_gpsd_usage()
{
    fprintf(stderr, "GPSD input:\n");
    fprintf(stderr, "--gpsd_address: gpsd address (r)\n");
    fprintf(stderr, "--gpsd_port: gpsd port (r)\n");
}

void md_gpsd_setup(struct md_exporter *mde, struct md_input_gpsd *mig)
{
    mig->parent = mde;
    mig->init = md_input_gpsd_init;
    mig->usage = md_gpsd_usage;
}
