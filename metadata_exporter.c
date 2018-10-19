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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <syslog.h>

#include <libmnl/libmnl.h>
#include JSON_LOC
#include <sys/time.h>

#include "metadata_exporter.h"
#ifdef GPSD_SUPPORT
    #include "metadata_input_gpsd.h"
#endif
#ifdef ZEROMQ_SUPPORT_INPUT
    #include "metadata_input_zeromq.h"
#endif
#ifdef ZEROMQ_SUPPORT_INPUT_RELAY
    #include "metadata_input_zeromq_relay.h"
#endif
#ifdef MUNIN_SUPPORT
    #include "metadata_input_munin.h"
#endif
#ifdef SYSEVENT_SUPPORT
    #include "metadata_input_sysevent.h"
#endif
#ifdef GPS_NSB_SUPPORT
    #include "metadata_input_gps_nsb.h"
#endif
#include "metadata_input_iface_test.h"
#ifdef SQLITE_SUPPORT
    #include "metadata_writer_sqlite.h"
#endif
#ifdef ZEROMQ_SUPPORT_WRITER
    #include "metadata_writer_zeromq.h"
#endif
#ifdef NNE_SUPPORT
    #include "metadata_writer_nne.h"
#endif
#ifdef NEAT_SUPPORT
    #include "metadata_writer_neat.h"
#endif

#ifdef FILE_SUPPORT
    #include "metadata_writer_file.h"
#endif

#include "backend_event_loop.h"
#include "metadata_exporter_log.h"

struct md_writer_file;
struct md_input_gpsd;
struct md_input_munin;
struct md_input_sysevent;
struct md_writer_sqlite;
struct md_writer_zeromq;

uint16_t mde_inc_seq(struct md_exporter *mde) {
    uint16_t retval = mde->seq++;

    //We use 0 as false value when checking if variable is present
    if (mde->seq == 0)
        mde->seq = 1;

    return retval;
}

void mde_publish_event_obj(struct md_exporter *mde, struct md_event *event)
{
    uint8_t i;

    //Iterate through every handler and pass the object
    for (i=0; i<=MD_WRITER_MAX; i++) {
        if (mde->md_writers[i] != NULL &&
            mde->md_writers[i]->handle != NULL)
            mde->md_writers[i]->handle((struct md_writer*) mde->md_writers[i],
                                       event);
    }
}

static void mde_itr_cb(void *ptr)
{
    struct md_exporter *mde = ptr;
    uint8_t i;

    for (i=0; i<=MD_WRITER_MAX; i++) {
        if (mde->md_writers[i] != NULL &&
            mde->md_writers[i]->itr_cb != NULL)
            mde->md_writers[i]->itr_cb(mde->md_writers[i]);
    }
}

//TODO: Refactor, create a backend helpers file?
void mde_start_timer(struct backend_event_loop *event_loop,
                     struct backend_timeout_handle *timeout_handle,
                     uint32_t timeout)
{
    struct timeval tv;
    uint64_t cur_time;

    gettimeofday(&tv, NULL);
    cur_time = (tv.tv_sec * 1e3) + (tv.tv_usec / 1e3);

    timeout_handle->timeout_clock = cur_time + timeout;
    backend_insert_timeout(event_loop, timeout_handle);
}

//TODO: Find a place to catch termination signals, and call this.
void mde_destroy(struct md_exporter *mde) 
{
    int i;
    for (i=0; i<=MD_INPUT_MAX; i++) 
        if (mde->md_inputs[i] != NULL) 
            if (mde->md_inputs[i]->destroy != NULL) 
                mde->md_inputs[i]->destroy(mde->md_inputs[i]);
}

static int configure_core(struct md_exporter **mde)
{
    //Configure core variables
    *mde = calloc(sizeof(struct md_exporter), 1);

    if (*mde == NULL) {
        fprintf(stderr, "could not allocate room for required data structures");
        return RETVAL_FAILURE;
    }

    if(!((*mde)->event_loop = backend_event_loop_create()))
        return RETVAL_FAILURE;

    (*mde)->seq = 1;
    (*mde)->event_loop->itr_cb = mde_itr_cb;
    (*mde)->event_loop->itr_data = *mde;
    (*mde)->logfile = stderr;

    return RETVAL_SUCCESS;
}

static void default_usage()
{
    fprintf(stderr, "Parameters. At least one input and one writer must be specified in the configuration.\n");
    fprintf(stderr, "Default:\n");
    fprintf(stderr, "-c: JSON configuration file for the metadata exporter\n");
    fprintf(stderr, "-h: Display usage of exporter, inputs and writers\n\n");
}

static void print_usage()
{
    default_usage();
    fprintf(stderr, "Configuration file syntax:\n");
    fprintf(stderr, "INPUTS:\n");
#ifdef GPSD_SUPPORT
    md_gpsd_usage();
#endif
#ifdef MUNIN_SUPPORT
    md_munin_usage();
#endif
#ifdef ZEROMQ_SUPPORT_INPUT_RELAY
    md_zeromq_relay_usage();
#endif
#ifdef GPS_NSB_SUPPORT
    md_gps_nsb_usage();
#endif
#ifdef SYSEVENT_SUPPORT
    md_sysevent_usage();
#endif
    fprintf(stderr, "WRITERS:\n");
#ifdef NEAT_SUPPORT
    md_neat_usage();
#endif
#ifdef NNE_SUPPORT
    md_nne_usage();
#endif
#ifdef SQLITE_SUPPORT
    md_sqlite_usage();
#endif
#ifdef ZEROMQ_SUPPORT_WRITER
    md_zeromq_writer_usage();
#endif
}

void read_config(char* config_file, json_object** config_obj)
{
    json_tokener *tok;
    json_object *parsed;
    char buffer[CONFIG_MAX_SIZE];

    FILE* config = fopen(config_file, "r");

    if (config == NULL) {     
        fprintf(stderr, "Could not open configuration file.\n");
        exit(EXIT_FAILURE);
    }
    size_t len = fread(buffer, sizeof(char), CONFIG_MAX_SIZE, config);
    if (len==0) {
        fprintf(stderr, "Could not read configuration file.\n");
        exit(EXIT_FAILURE);
    } else {
        buffer[len] = '\0';
    }
    fclose(config);

    tok = json_tokener_new();

    if (tok == NULL) {
        fprintf(stderr, "Could not create JSON tokener.\n");
        exit(EXIT_FAILURE);
    }

    parsed = json_tokener_parse_ex(tok, buffer, len);
    if (parsed == NULL) {
        fprintf(stderr, "Could not parse configuration file.\n");
        exit(EXIT_FAILURE);
    }
    json_tokener_free(tok);

    *config_obj = parsed;
}

int main(int argc, char *argv[])
{
    struct md_exporter *mde;
    int32_t i;
    uint8_t num_writers = 0, num_inputs = 0;
    const char *logfile_path = NULL;
    json_object *config = NULL;

    //Try to configure core before we set up the outputters
    if (configure_core(&mde))
        exit(EXIT_FAILURE);

    //Process core options, short options allowed. We do this here since we need
    //an allocated writers array
    opterr = 0;
    while ((i = getopt(argc, argv, "c:h")) != -1) {
        if (i == -1) {
            break;
        } else if (i == 'c') {
            read_config(optarg, &config);
        } else if (i == 'h') { 
            print_usage();
            exit(EXIT_SUCCESS);
        }
    }

    if (config == NULL) {
        META_PRINT_SYSLOG(mde, LOG_ERR, "Parameter -c is required to run.\n");
        exit(EXIT_FAILURE);
    }

    json_object_object_foreach(config, key, val) {
        if (!strcmp(key, "logfile")) {
            logfile_path = json_object_get_string(val); 
        } else if (!strcmp(key, "syslog")) {
            mde->use_syslog = json_object_get_int(val);
        }
#ifdef GPS_NSB_SUPPORT
        else if (!strcmp(key, "gps_nsb")) {
            mde->md_inputs[MD_INPUT_GPS_NSB] = calloc(sizeof(struct md_input_gps_nsb), 1);

            if (mde->md_inputs[MD_INPUT_GPS_NSB] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate NSB GPS input\n");
                exit(EXIT_FAILURE);
            }

            md_gps_nsb_setup(mde, (struct md_input_gps_nsb*) mde->md_inputs[MD_INPUT_GPS_NSB]);
            num_inputs++;
        }
#endif
#ifdef ZEROMQ_SUPPORT_INPUT
        else if (!strcmp(key, "zmq_input")) {
            mde->md_inputs[MD_INPUT_ZEROMQ] = calloc(sizeof(struct md_input_zeromq), 1);

            if (mde->md_inputs[MD_INPUT_ZEROMQ] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate ZeroMQ input\n");
                exit(EXIT_FAILURE);
            }

            md_zeromq_input_setup(mde, (struct md_input_zeromq*) mde->md_inputs[MD_INPUT_ZEROMQ]);
            num_inputs++;
        }
#endif
#ifdef ZEROMQ_SUPPORT_INPUT_RELAY
        else if (!strcmp(key, "zmq_input_relay")) {
            mde->md_inputs[MD_INPUT_ZEROMQ_RELAY] = calloc(sizeof(struct md_input_zeromq_relay), 1);

            if (mde->md_inputs[MD_INPUT_ZEROMQ_RELAY] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate ZeroMQ Relay input\n");
                exit(EXIT_FAILURE);
            }

            md_zeromq_relay_setup(mde, (struct md_input_zeromq_relay*) mde->md_inputs[MD_INPUT_ZEROMQ_RELAY]);
            num_inputs++;
        }
#endif
#ifdef NNE_SUPPORT
        else if (!strcmp(key, "nne")) {
            mde->md_writers[MD_WRITER_NNE] = calloc(sizeof(struct md_writer_nne), 1);

            if (mde->md_writers[MD_WRITER_NNE] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate NNE  writer\n");
                exit(EXIT_FAILURE);
            }

            md_nne_setup(mde, (struct md_writer_nne*) mde->md_writers[MD_WRITER_NNE]);
            num_writers++;
        }
#endif
#ifdef NEAT_SUPPORT
        else if (!strcmp(key, "neat")) {
            mde->md_writers[MD_WRITER_NEAT] = calloc(sizeof(struct md_writer_neat), 1);

            if (mde->md_writers[MD_WRITER_NEAT] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate NEAT writer\n");
                exit(EXIT_FAILURE);
            }

            md_neat_setup(mde, (struct md_writer_neat *)mde->md_writers[MD_WRITER_NEAT]);
            num_writers++;
        }
#endif
#ifdef GPSD_SUPPORT
        else if (!strcmp(key, "gpsd")) {
            mde->md_inputs[MD_INPUT_GPSD] = calloc(sizeof(struct md_input_gpsd), 1);

            if (mde->md_inputs[MD_INPUT_GPSD] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate GPSD input\n");
                exit(EXIT_FAILURE);
            }

            md_gpsd_setup(mde, (struct md_input_gpsd*) mde->md_inputs[MD_INPUT_GPSD]);
            num_inputs++;
        }
#endif
#ifdef MUNIN_SUPPORT
        else if (!strcmp(key, "munin")) {
            mde->md_inputs[MD_INPUT_MUNIN] = calloc(sizeof(struct md_input_munin), 1);

            if (mde->md_inputs[MD_INPUT_MUNIN] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate Munin input\n");
                exit(EXIT_FAILURE);
            }

            md_munin_setup(mde, (struct md_input_munin*) mde->md_inputs[MD_INPUT_MUNIN]);
            num_inputs++;
        }
#endif
#ifdef SYSEVENT_SUPPORT
        else if (!strcmp(key, "sysevent")) {
            mde->md_inputs[MD_INPUT_SYSEVENT] = calloc(sizeof(struct md_input_sysevent), 1);

            if (mde->md_inputs[MD_INPUT_SYSEVENT] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate Sysevent input\n");
                exit(EXIT_FAILURE);
            }

            md_sysevent_setup(mde, (struct md_input_sysevent*) mde->md_inputs[MD_INPUT_SYSEVENT]);
            num_inputs++;
        } 
#endif 
#ifdef SQLITE_SUPPORT
        else if (!strcmp(key, "sqlite")) {
            mde->md_writers[MD_WRITER_SQLITE] = calloc(sizeof(struct md_writer_sqlite), 1);

            if (mde->md_writers[MD_WRITER_SQLITE] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate SQLite writer\n");
                exit(EXIT_FAILURE);
            }

            md_sqlite_setup(mde, (struct md_writer_sqlite*) mde->md_writers[MD_WRITER_SQLITE]);
            num_writers++;
        }
#endif
#ifdef ZEROMQ_SUPPORT_WRITER
        else if (!strcmp(key, "zmq")) {
            mde->md_writers[MD_WRITER_ZEROMQ] = calloc(sizeof(struct md_writer_zeromq), 1);

            if (mde->md_writers[MD_WRITER_ZEROMQ] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate ZMQ writer\n");
                exit(EXIT_FAILURE);
            }

            md_zeromq_writer_setup(mde, (struct md_writer_zeromq*) mde->md_writers[MD_WRITER_ZEROMQ]);
            num_writers++;
        }
#endif
#ifdef FILE_SUPPORT
        else if (!strcmp(key, "file")) {
            mde->md_writers[MD_WRITER_FILE] = calloc(sizeof(struct md_writer_file), 1);

            if (mde->md_writers[MD_WRITER_FILE] == NULL) {
                META_PRINT_SYSLOG(mde, LOG_ERR, "Could not allocate file writer\n");
                exit(EXIT_FAILURE);
            }

            md_file_setup(mde, (struct md_writer_file*) mde->md_writers[MD_WRITER_FILE]);
            num_writers++;
        }
#endif
    }

    if (num_writers == 0 || num_inputs == 0) {
        fprintf(stderr, "No input(s)/writer(s) specified\n");
        exit(EXIT_FAILURE);
    }

    if (logfile_path) {
        mde->logfile = fopen(logfile_path, "a");

        if (mde->logfile == NULL) {
            fprintf(stderr, "Could not open logfile: %s\n", logfile_path);
            exit(EXIT_FAILURE);
        }
    }

    for (i=0; i<=MD_INPUT_MAX; i++) {
        if (mde->md_inputs[i] != NULL) {
            META_PRINT_SYSLOG(mde, LOG_INFO, "Will configure input %d\n", i);
            //glic requires optind to be 0 for internal state to be reset when
            //using extensions
            optind = 0;
            if (mde->md_inputs[i]->init(mde->md_inputs[i], config))
                exit(EXIT_FAILURE);
        }
    }

    for (i=0; i<=MD_WRITER_MAX; i++) {
        if (mde->md_writers[i] != NULL) {
            META_PRINT_SYSLOG(mde, LOG_INFO, "Will configure writer %d\n", i);
            //glic requires optind to be 0 for internal state to be reset when
            //using extensions
            optind = 0;
            if (mde->md_writers[i]->init(mde->md_writers[i], config))
                exit(EXIT_FAILURE);
        }
    }

    json_object_put(config);

    backend_event_loop_run(mde->event_loop);

    META_PRINT_SYSLOG(mde, LOG_ERR, "Threads should NEVER exit\n");
    exit(EXIT_FAILURE);
}
