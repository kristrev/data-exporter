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

#include <libmnl/libmnl.h>
#include JSON_LOC
#include <sys/time.h>

#include "metadata_exporter.h"
#ifdef GPSD_SUPPORT
    #include "metadata_input_gpsd.h"
#endif
#ifdef MUNIN_SUPPORT
    #include "metadata_input_munin.h"
#endif
#ifdef NSB_GPS
    #include "metadata_input_gps_nsb.h"
#endif
#include "metadata_input_netlink.h"
#ifdef SQLITE_SUPPORT
    #include "metadata_writer_sqlite.h"
#endif
#ifdef ZEROMQ_SUPPORT
    #include "metadata_writer_zeromq.h"
#endif
#ifdef NNE_SUPPORT
    #include "metadata_writer_nne.h"
#endif

#include "netlink_helpers.h"
#include "backend_event_loop.h"
#include "metadata_exporter_log.h"

struct md_input_gpsd;
struct md_input_munin;
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

static struct json_object *create_fake_gps_gga_obj()
{
    struct json_object *obj = NULL, *obj_add = NULL;
    struct timeval tv;

    if (!(obj = json_object_new_object()))
        return NULL;

    gettimeofday(&tv, NULL);
	if (!(obj_add = json_object_new_int64(tv.tv_sec))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "timestamp", obj_add);

    if (!(obj_add = json_object_new_int(META_TYPE_POS))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "event_type", obj_add);

    if (!(obj_add = json_object_new_string("$GPGGA,190406.0,5103.732280,N,01701.493660,E,1,05,1.7,130.0,M,42.0,M,,*53"))) {
        json_object_put(obj);
        return NULL;
    }

    json_object_object_add(obj, "nmea_string", obj_add);

    return obj;
}

static struct json_object *create_fake_gps_rmc_obj()
{
    struct json_object *obj = NULL, *obj_add = NULL;
    struct timeval tv;

    if (!(obj = json_object_new_object()))
        return NULL;

    gettimeofday(&tv, NULL);
	if (!(obj_add = json_object_new_int64(tv.tv_sec))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "timestamp", obj_add);

    if (!(obj_add = json_object_new_int(META_TYPE_POS))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "event_type", obj_add);

    if (!(obj_add = json_object_new_string("$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68"))) {
        json_object_put(obj);
        return NULL;
    }

    json_object_object_add(obj, "nmea_string", obj_add);

    return obj;
}

static struct json_object *create_fake_conn_obj(uint64_t l3_id, uint64_t l4_id,
        uint8_t event_param, char *event_value_str)
{
	struct timeval tv;
	struct json_object *obj = NULL, *obj_add = NULL;
    uint8_t rand_value = 0;
    uint64_t rand_value_64 = 0;

	if (!(obj = json_object_new_object()))
		return NULL;


	gettimeofday(&tv, NULL);
	if (!(obj_add = json_object_new_int64(tv.tv_sec))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "timestamp", obj_add);

    if (l3_id)
        rand_value_64 = l3_id;
    else
        rand_value_64 = (uint64_t) random();

    if (!(obj_add = json_object_new_int64(rand_value_64))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "l3_session_id", obj_add);

    if (l4_id)
        rand_value_64 = l4_id;
    else
        rand_value_64 = (uint64_t) random();

    if (!(obj_add = json_object_new_int64(rand_value_64))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "l4_session_id", obj_add);

    if (!(obj_add = json_object_new_int(META_TYPE_CONNECTION))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "event_type", obj_add);

    rand_value = (uint8_t) random();
    if (!(obj_add = json_object_new_int(rand_value))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "interface_type", obj_add);

    rand_value = (uint8_t) random();
    if (!(obj_add = json_object_new_int(rand_value))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "network_address_family", obj_add);

    if (!event_param)
        rand_value = (uint8_t) random();
    else
        rand_value = event_param;

    if (!(obj_add = json_object_new_int(rand_value))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "event_param", obj_add);

    if (event_value_str) {
        if (!(obj_add = json_object_new_string(event_value_str))) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, "event_value_str", obj_add);
    } else {
        rand_value = (uint8_t) random();
        if (!(obj_add = json_object_new_int(rand_value))) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, "event_value", obj_add);
    }

    rand_value = (uint8_t) random();
    if (!(obj_add = json_object_new_int(rand_value))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "interface_id_type", obj_add);

    if (!(obj_add = json_object_new_string("89470000140710276612"))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "interface_id", obj_add);

    if (!(obj_add = json_object_new_string("192.168.0.153/24"))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "network_address", obj_add);

    rand_value = (uint8_t) random();
    if (rand_value % 2) {
        obj_add = json_object_new_int(24201);

        if (!obj_add) {
            json_object_put(obj);
            return NULL;
        }

        json_object_object_add(obj, "network_provider", obj_add);
    }

    if (!(obj_add = json_object_new_int(-99))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, "signal_strength", obj_add);

	return obj;	
}

//Test function which just generates some netlink messages that are sent to our
//group
static void test_netlink(uint32_t packets)
{
    struct mnl_socket *mnl_sock = NULL;
    struct sockaddr_nl netlink_addr;
	uint8_t snd_buf[MNL_SOCKET_BUFFER_SIZE];
    socklen_t netlink_addrlen = sizeof(netlink_addr);
    struct nlmsghdr *netlink_hdr;
    uint16_t cnt = 0;
    uint32_t i = 0;
    ssize_t retval;
	struct json_object *obj_to_send = NULL;
    const char *json_str;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    mnl_sock = nlhelper_create_socket(NETLINK_USERSOCK, 0);

    if (mnl_sock == NULL) {
        fprintf(stderr, "Could not create netlink socket used for testing\n");
        return;
    }

    memset(&netlink_addr, 0, sizeof(netlink_addr));
    memset(snd_buf, 0, sizeof(snd_buf));

    netlink_hdr = mnl_nlmsg_put_header(snd_buf);
    netlink_hdr->nlmsg_type = 1;

    netlink_addr.nl_family = AF_NETLINK;

    //A message is broadcasted (multicasted) to all members of the group, except
    //the one where portid equals nl_pid (if any). Then it is unicasted to the
    //socket where portid equals nl_pid (if any). See af_netlink.c and
    //netlink_unicast()/netlink_broadcast().
    //
    //When testing, there is no need to multicast. We can just send to the PID
    netlink_addr.nl_pid = getpid();

    //TODO: Specify number of packets from command line
    while(1) {
        if (cnt == 0)
            obj_to_send = create_fake_conn_obj(1, 2, CONN_EVENT_META_UPDATE, "1,2,1,");
        else
            obj_to_send = create_fake_conn_obj(2, 3, CONN_EVENT_META_UPDATE, "1,2,1,4");

        if (!obj_to_send)
            continue;

        //Every applcation will export json
        //TODO: Refactor/split all of this code into two functions
        json_str = json_object_to_json_string_ext(obj_to_send, JSON_C_TO_STRING_PLAIN);
        memcpy(netlink_hdr + 1, json_str, strlen(json_str) + 1);
        netlink_hdr->nlmsg_len = mnl_nlmsg_size(MNL_ALIGN(strlen(json_str)));
        json_object_put(obj_to_send);

        retval = sendto(mnl_socket_get_fd(mnl_sock),
                        snd_buf,
                        netlink_hdr->nlmsg_len,
                        0,
                        (struct sockaddr*) &netlink_addr,
                        netlink_addrlen);

        obj_to_send = create_fake_gps_gga_obj();
        json_str = json_object_to_json_string_ext(obj_to_send, JSON_C_TO_STRING_PLAIN);
        memcpy(netlink_hdr + 1, json_str, strlen(json_str) + 1);
        netlink_hdr->nlmsg_len = mnl_nlmsg_size(MNL_ALIGN(strlen(json_str)));
        json_object_put(obj_to_send);

        retval = sendto(mnl_socket_get_fd(mnl_sock),
                        snd_buf,
                        netlink_hdr->nlmsg_len,
                        0,
                        (struct sockaddr*) &netlink_addr,
                        netlink_addrlen);

        obj_to_send = create_fake_gps_rmc_obj();
        json_str = json_object_to_json_string_ext(obj_to_send, JSON_C_TO_STRING_PLAIN);
        memcpy(netlink_hdr + 1, json_str, strlen(json_str) + 1);
        netlink_hdr->nlmsg_len = mnl_nlmsg_size(MNL_ALIGN(strlen(json_str)));
        json_object_put(obj_to_send);

        retval = sendto(mnl_socket_get_fd(mnl_sock),
                        snd_buf,
                        netlink_hdr->nlmsg_len,
                        0,
                        (struct sockaddr*) &netlink_addr,
                        netlink_addrlen);

        if (retval > 0)
            printf("Sent %u packets\n", ++cnt);

        if (packets && (++i >= packets))
            break;

        usleep(1000000);
    }
}

static void *mde_run(void *ptr)
{
    struct md_exporter *mde = ptr;
    backend_event_loop_run(mde->event_loop);
    return NULL;
}

static void run_test_mode(struct md_exporter *mde, uint32_t packets)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&thread, &attr, mde_run, mde);

    test_netlink(packets);

    pthread_join(thread, NULL);

    META_PRINT(mde->logfile, "Threads should NEVER exit\n");
}

static void default_usage()
{
    fprintf(stderr, "Support parameters (r is required). At least one input and one writer must be specified.\n");
    fprintf(stderr, "Default:\n");
    fprintf(stderr, "--netlink/-n: netlink input\n");
#ifdef NSB_GPS
    fprintf(stderr, "--nsb_gps: NSB NMEA GPS input\n");
#endif
#ifdef GPSD_SUPPORT
    fprintf(stderr, "--gpsd/-g: gpsd input\n");
#endif
#ifdef MUNIN_SUPPORT
    fprintf(stderr, "--munin/-m: munin input\n");
#endif
#ifdef SQLITE_SUPPORT
    fprintf(stderr, "--sqlite/-s: sqlite writer\n");
#endif
#ifdef ZEROMQ_SUPPORT
    fprintf(stderr, "--zeromq/-z: zeromq writer\n");
#endif
#ifdef NNE_SUPPORT
    fprintf(stderr, "--nne: nornet edge measurement writer\n");
#endif
    fprintf(stderr, "--test/-t: test mode, application generates fake data that is handled by writers\n");
    fprintf(stderr, "--packets/-p: number of packets that will be generated in debug mode (default: infinite)\n");
    fprintf(stderr, "--logfile/-l: path to logfile (default: stderr)\n");
    fprintf(stderr, "--help/-h: Display usage of exporter and specified writers\n");
}

static void print_usage(struct md_exporter *mde)
{
    int32_t i;

    default_usage();

    for (i=0; i<=MD_WRITER_MAX; i++) {
        if (mde->md_writers[i] != NULL &&
            mde->md_writers[i]->usage)
            mde->md_writers[i]->usage();
    }

    for (i=0; i<=MD_INPUT_MAX; i++) {
        if (mde->md_inputs[i] != NULL &&
            mde->md_inputs[i]->usage)
            mde->md_inputs[i]->usage();
    }
}

int main(int argc, char *argv[])
{
    struct md_exporter *mde;
    int32_t i, option_index = 0;
    uint32_t packets = 0;
    uint8_t test_mode = 0, show_help = 0, num_writers = 0, num_inputs = 0;
    const char *logfile_path = NULL;

    static struct option core_options[] = {
        {"netlink",      no_argument,        0,  'n'},
#ifdef SQLITE_SUPPORT
        {"sqlite",       no_argument,        0,  's'},
#endif
#ifdef NSB_GPS
        {"nsb_gps",      no_argument,        0,  0  },
#endif
#ifdef ZEROMQ_SUPPORT
        {"zeromq",       no_argument,        0,  'z'},
#endif
#ifdef NNE_SUPPORT
        {"nne",          no_argument,        0,  0  },
#endif
#ifdef GPSD_SUPPORT
        {"gpsd",         no_argument,        0,  'g'},
#endif
#ifdef MUNIN_SUPPORT
        {"munin",         no_argument,       0,  'm'},
#endif
        {"packets",      required_argument,  0,  'p'},
        {"test",         no_argument,        0,  't'},
        {"logfile",      required_argument,  0,  'l'},
        {"help",         no_argument,        0,  'h'},
        {0,              0,                  0,   0 }};

    //Try to configure core before we set up the outputters
    if (configure_core(&mde))
        exit(EXIT_FAILURE);

    //Process core options, short options allowed. We do this here since we need
    //an allocated writers array
    opterr = 0;
    while (1) {
        //Use glic extension to avoid getopt permuting array while processing
        i = getopt_long(argc, argv, "--szhmgtnp:l:", core_options, &option_index);

        if (i == -1)
            break;

        if (i == 0) {
#ifdef NSB_GPS
            if (strcmp(core_options[option_index].name, "nsb_gps") == 0) {
                mde->md_inputs[MD_INPUT_GPS_NSB] = calloc(sizeof(struct md_input_gps_nsb), 1);

                if (mde->md_inputs[MD_INPUT_GPS_NSB] == NULL) {
                    META_PRINT(mde->logfile, "Could not allocate Netlink input\n");
                    exit(EXIT_FAILURE);
                }

                md_gps_nsb_setup(mde, (struct md_input_gps_nsb*) mde->md_inputs[MD_INPUT_GPS_NSB]);
                num_inputs++;
            }
#endif
#ifdef NNE_SUPPORT
            if (strcmp(core_options[option_index].name, "nne") == 0) {
                mde->md_writers[MD_WRITER_NNE] = calloc(sizeof(struct md_writer_nne), 1);

                if (mde->md_writers[MD_WRITER_NNE] == NULL) {
                    META_PRINT(mde->logfile, "Could not allocate NNE  writer\n");
                    exit(EXIT_FAILURE);
                }

                md_nne_setup(mde, (struct md_writer_nne*) mde->md_writers[MD_WRITER_NNE]);
                num_writers++;
            }
#endif
            continue;
        }

        switch (i) {
        case 'n':
            mde->md_inputs[MD_INPUT_NETLINK] = calloc(sizeof(struct md_input_netlink),1);

            if (mde->md_inputs[MD_INPUT_NETLINK] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate Netlink input\n");
                exit(EXIT_FAILURE);
            }

            md_netlink_setup(mde, (struct md_input_netlink*) mde->md_inputs[MD_INPUT_NETLINK]);
            num_inputs++;
            break;
#ifdef GPSD_SUPPORT
        case 'g':
            mde->md_inputs[MD_INPUT_GPSD] = calloc(sizeof(struct md_input_gpsd), 1);

            if (mde->md_inputs[MD_INPUT_GPSD] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate GPSD input\n");
                exit(EXIT_FAILURE);
            }

            md_gpsd_setup(mde, (struct md_input_gpsd*) mde->md_inputs[MD_INPUT_GPSD]);
            num_inputs++;
            break;
#endif
#ifdef MUNIN_SUPPORT
        case 'm':
            mde->md_inputs[MD_INPUT_MUNIN] = calloc(sizeof(struct md_input_munin), 1);

            if (mde->md_inputs[MD_INPUT_MUNIN] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate Munin input\n");
                exit(EXIT_FAILURE);
            }

            md_munin_setup(mde, (struct md_input_munin*) mde->md_inputs[MD_INPUT_MUNIN]);
            num_inputs++;
            break;
#endif
#ifdef SQLITE_SUPPORT
        case 's':
            mde->md_writers[MD_WRITER_SQLITE] = calloc(sizeof(struct md_writer_sqlite), 1);

            if (mde->md_writers[MD_WRITER_SQLITE] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate SQLite writer\n");
                exit(EXIT_FAILURE);
            }

            md_sqlite_setup(mde, (struct md_writer_sqlite*) mde->md_writers[MD_WRITER_SQLITE]);
            num_writers++;
            break;
#endif
#ifdef ZEROMQ_SUPPORT
        case 'z':
            mde->md_writers[MD_WRITER_ZEROMQ] = calloc(sizeof(struct md_writer_zeromq), 1);

            if (mde->md_writers[MD_WRITER_ZEROMQ] == NULL) {
                META_PRINT(mde->logfile, "Could not allocate SQLite writer\n");
                exit(EXIT_FAILURE);
            }

            md_zeromq_setup(mde, (struct md_writer_zeromq*) mde->md_writers[MD_WRITER_ZEROMQ]);
            num_writers++;
            break;
#endif
        case 't':
            test_mode = 1;
            break;
        case 'p':
            packets = (uint32_t) atoi(optarg);
            break;
        case 'l':
            logfile_path = optarg;
            break;
        case 'h':
            show_help = 1;
            break;
        }
    }

    if (show_help) {
        print_usage(mde);
        exit(EXIT_SUCCESS);
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
            META_PRINT(mde->logfile, "Will configure input %d\n", i);
            //glic requires optind to be 0 for internal state to be reset when
            //using extensions
            optind = 0;
            if (mde->md_inputs[i]->init(mde->md_inputs[i], argc, argv))
                exit(EXIT_FAILURE);
        }
    }

    for (i=0; i<=MD_WRITER_MAX; i++) {
        if (mde->md_writers[i] != NULL) {
            META_PRINT(mde->logfile, "Will configure writer %d\n", i);
            //glic requires optind to be 0 for internal state to be reset when
            //using extensions
            optind = 0;
            if (mde->md_writers[i]->init(mde->md_writers[i], argc, argv))
                exit(EXIT_FAILURE);
        }
    }

    if (test_mode)
        run_test_mode(mde, packets);
    else
        backend_event_loop_run(mde->event_loop);

    META_PRINT(mde->logfile, "Threads should NEVER exit\n");
    exit(EXIT_FAILURE);
}
