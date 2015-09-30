/* Copyright (c) 2015, Celerway, Tomasz Rozensztrauch <t.rozensztrauch@radytek.com>
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
#include <sys/time.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>

#include "metadata_writer_nne.h"
#include "backend_event_loop.h"

#define NNE_DEFAULT_INTERVAL_MS 15000
#define NNE_DEFAULT_DIRECTORY   "/nne/data/"
#define NNE_DEFAULT_PREFIX      "gps_"
#define NNE_DEFAULT_EXTENSION   ".sdat"

static uint8_t md_nne_handle_gps_event(struct md_writer_nne *mwn,
                                       struct md_gps_event *mge) {
    char name_buf[256];
    char tm_buf[128];
    char xml_buf[128];
    struct tm *gtm;
    size_t retval;

    if (mge->minmea_id == MINMEA_SENTENCE_RMC)
        return RETVAL_IGNORE;

    if (mwn->dat_file == NULL) {
        retval = snprintf(name_buf, sizeof(name_buf), "%s%s%d%s",
                          mwn->directory, mwn->prefix,
                          mwn->instance_id, mwn->extension);
        if (retval >= sizeof(name_buf)) {
            fprintf(stderr, "NNE writer: Failed to format export file name\n");
            return RETVAL_FAILURE;
        }

        if ((mwn->dat_file = fopen(name_buf, "a")) == NULL) {
            fprintf(stderr, "NNE writer: Failed to open export file %s\n",
                    name_buf);
            return RETVAL_FAILURE;
        }
    }

    if ((gtm = gmtime((time_t *)&mge->tstamp)) == NULL) {
        fprintf(stderr, "NNE writer: Invalid GPS timestamp\n");
        return RETVAL_FAILURE;
    }    

    if (!strftime(tm_buf, sizeof(tm_buf), "%Y-%m-%d %H:%M:%S", gtm)) {
        fprintf(stderr, "NNE writer: Failed to format timestamp\n");
        return RETVAL_FAILURE;
    }    

    retval = snprintf(xml_buf, sizeof(xml_buf),
                      "<d e=\"0\"><lat>%f</lat><lon>%f</lon><speed>%f</speed></d>",
                      mge->latitude, mge->longitude, mge->speed);
    if (retval >= sizeof(xml_buf)) {
        fprintf(stderr, "NNE writer: Failed to format XML data\n");
        return RETVAL_FAILURE;
    }

    mwn->sequence += 1;

    if (fprintf(mwn->dat_file, "%s\t%i\t%i\t%s\n", tm_buf, mwn->instance_id,
                mwn->sequence, xml_buf) < 0) {
        fprintf(stderr, "NNE writer: Failed to write to export file%s\n",
                name_buf);
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

static void md_nne_handle_timeout(void *ptr) {
    struct md_writer_nne *mwn = ptr;
    struct timeval tv;
    char tm_buf[128];
    char src_buf[256];
    char dst_buf[256];
    struct tm *gtm;
    size_t retval;

    if (mwn->dat_file != NULL) {
        fclose(mwn->dat_file);
        mwn->dat_file = NULL;

        if (gettimeofday(&tv, NULL)) {
            fprintf(stderr, "NNE writer: Failed to obtain current time\n");
            return;
        }

        if ((gtm = gmtime((time_t *)&tv.tv_sec)) == NULL) {
            fprintf(stderr, "NNE writer: Invalid current timestamp\n");
            return;
        }

        if (!strftime(tm_buf, sizeof(tm_buf), "%Y-%m-%d_%H-%M-%S", gtm)) {
            fprintf(stderr, "md_nne_handle_timeout: ERROR strftime\n");
            return;
        }

        retval = snprintf(src_buf, sizeof(src_buf), "%s%s%d%s",
                          mwn->directory, mwn->prefix,
                          mwn->instance_id, mwn->extension);
        if (retval >= sizeof(src_buf)) {
            fprintf(stderr, "NNE writer: Failed to format export file name\n");
            return;
        }

        retval = snprintf(dst_buf, sizeof(dst_buf), "%s.%s",
                          src_buf, tm_buf);
        if (retval >= sizeof(dst_buf)) {
            fprintf(stderr, "NNE writer: Failed to format rotated export file name\n");
            return;
        }

        if (rename(src_buf, dst_buf)) {
            fprintf(stderr, "NNE writer: Failed to rename file %s to %s\n",
                    src_buf, dst_buf);
            return;
        }

        fprintf(stderr, "NNE writer: Rotated file %s to %s\n",
                src_buf, dst_buf);
    }
}

static int32_t md_nne_init(void *ptr, int argc, char *argv[]) {
    struct md_writer_nne *mwn = ptr;
    int c, option_index;

    mwn->dat_file = NULL;
    mwn->sequence = 0;
    memset(mwn->directory, 0, sizeof(mwn->directory));
    memset(mwn->prefix, 0, sizeof(mwn->prefix));
    mwn->interval = NNE_DEFAULT_INTERVAL_MS;
    mwn->instance_id = 0;
    memset(mwn->extension, 0, sizeof(mwn->extension));

    strcpy(mwn->directory, NNE_DEFAULT_DIRECTORY);
    strcpy(mwn->prefix, NNE_DEFAULT_PREFIX);
    strcpy(mwn->extension, NNE_DEFAULT_EXTENSION);

    static struct option nne_options[] = {
        {"nne_interval",   required_argument, 0, 0},
        {"nne_instance",    required_argument, 0, 0},
        {"nne_gps_prefix", required_argument, 0, 0},
        {0,                0,                 0, 0}
    };

    while (1) {
        c = getopt_long_only(argc, argv, "--", nne_options, &option_index);
        if (c == -1)
            break;
        else if (c)
            continue;

        if (!strcmp(nne_options[option_index].name, "nne_interval"))
            mwn->interval = ((uint32_t) atoi(optarg)) * 1000;
        else if (!strcmp(nne_options[option_index].name, "nne_instance"))
            mwn->instance_id = (uint32_t) atoi(optarg);
        else if (!strcmp(nne_options[option_index].name, "nne_gps_prefix")) {
            if (strlen(optarg) > sizeof(mwn->prefix) - 1) {
                fprintf(stderr, "NNE writer: gps prefix too long (>%d)\n",
                        sizeof(mwn->prefix) - 1);
                return RETVAL_FAILURE;
            }
            else
                strcpy(mwn->prefix, optarg);
        }
    }

    if (mwn->instance_id == 0) {
        fprintf(stderr, "NNE writer: Missing required argument --nne_instance\n");
        return RETVAL_FAILURE;
    }

    if(!(mwn->timeout_handle = backend_event_loop_create_timeout(0,                                                                                                                                                                      
             md_nne_handle_timeout, mwn, mwn->interval))) {
        fprintf(stdout, "NNE writer: Failed to create file rotation/export timeout\n");
        return RETVAL_FAILURE;  
    }  

    mde_start_timer(mwn->parent->event_loop, mwn->timeout_handle,
                    mwn->interval);

    return RETVAL_SUCCESS;
}

static void md_nne_handle(struct md_writer *writer, struct md_event *event) {
    struct md_writer_nne *mwn = (struct md_writer_nne*) writer;

    switch (event->md_type) {
        case META_TYPE_POS:
            md_nne_handle_gps_event(mwn, (struct md_gps_event*) event);
            break;
        default:
            return;
        }
}

static void md_nne_usage() {
    fprintf(stderr, "md_nne_usage\n");
    fprintf(stderr, "Nornet Edge writer.\n");
    fprintf(stderr, "--nne_interval: File rotation/export interval (in seconds)\n");
    fprintf(stderr, "--nne_instance: NNE measurement instance id\n");
    fprintf(stderr, "--nne_gps_prefix: File prefix /nne/data/<PREFIX><INSTANCE>.sdat\n");
}

void md_nne_setup(struct md_exporter *mde, struct md_writer_nne* mwn) {
    fprintf(stderr, "md_nne_setup\n");
    mwn->parent = mde;
    mwn->init = md_nne_init;
    mwn->handle = md_nne_handle;
    mwn->usage = md_nne_usage;
}

