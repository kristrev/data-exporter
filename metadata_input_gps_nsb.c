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

static void md_input_gps_nsb_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_gps_nsb *mign = ptr;
    char rcv_buf[4096];
    struct md_gps_event gps_event;
    int8_t sentence_id = 0;
    struct minmea_sentence_rmc rmc;
    int32_t retval;
    struct timeval tv;

    retval = recv(fd, rcv_buf, sizeof(rcv_buf), 0);

    if (retval <= 0)
        return;

    gettimeofday(&tv, NULL);
    sentence_id = minmea_sentence_id(rcv_buf, 0);

    if (sentence_id <= 0)
        return;

    memset(&gps_event, 0, sizeof(struct md_gps_event));
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
    gps_event.tstamp = tv.tv_sec;
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
        fprintf(stderr, "Could not get address info for NSB GPS\n");
        return RETVAL_FAILURE;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket for NSB GPS\n");
        return RETVAL_FAILURE;
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen)) {
        fprintf(stderr, "Failed to bind NSB GPS socket\n");
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
        fprintf(stderr, "Missing required GPSD argument\n");
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
