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
#include "metadata_input_netlink.h"
#include "metadata_input_nl_zmq_common.h"
#include "netlink_helpers.h"
#include "backend_event_loop.h"

#include "lib/minmea.h"
#include "metadata_exporter_log.h"

static void md_input_netlink_handle_iface_event(struct md_input_netlink *min,
        struct json_object *obj)
{
    init_iface_event(min->mie);

    if (parse_iface_event(obj, min->mie, min->parent) == RETVAL_FAILURE)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) min->mie);
}

static void md_input_netlink_radio_cell_loc_geran(struct md_input_netlink *min,
        struct json_object *obj)
{
    struct md_radio_cell_loc_geran_event * event = radio_cell_loc_geran(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_grr_cell_resel(struct md_input_netlink *min,
        struct json_object *obj)
{
    struct md_radio_grr_cell_resel_event *event = radio_grr_cell_resel(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_gsm_rr_cell_sel_reset_param(struct md_input_netlink *min,
        struct json_object *obj)
{
    struct md_radio_gsm_rr_cell_sel_reset_param_event *event = radio_gsm_rr_cell_sel_reset_param(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_gsm_rr_cipher_mode(struct md_input_netlink *min,
        struct json_object *obj)
{
   struct md_radio_gsm_rr_cipher_mode_event *event = radio_gsm_rr_cipher_mode(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_gsm_rr_channel_conf(struct md_input_netlink *min,
        struct json_object *obj)
{
    struct md_radio_gsm_rr_channel_conf_event* event = radio_gsm_rr_channel_conf(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_wcdma_rrc_state(struct md_input_netlink *min,
        struct json_object *obj)
{
   struct md_radio_wcdma_rrc_state_event *event = radio_wcdma_rrc_state(obj);

   if (!event)
       return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_radio_wcdma_cell_id(struct md_input_netlink *min,
        struct json_object *obj)
{
    struct md_radio_wcdma_cell_id_event *event = radio_wcdma_cell_id(obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) event);
    free(event);
}

static void md_input_netlink_handle_radio_event(struct md_input_netlink *min,
        struct json_object *obj)
{
    json_object *event_param_json;
    uint8_t event_param;

    if (!json_object_object_get_ex(obj, "event_param", &event_param_json)) {
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "Missing event type\n");
        return;
    }

    memset(min->mre, 0, sizeof(struct md_radio_event));
    min->mre->md_type = META_TYPE_RADIO;
    event_param = (uint8_t) json_object_get_int(event_param_json);

    switch (event_param) {
    case RADIO_EVENT_GSM_RR_CIPHER_MODE:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "GSM_RR_CIPHER_MODE\n");
        md_input_netlink_radio_gsm_rr_cipher_mode(min, obj);
        break;
    case RADIO_EVENT_GSM_RR_CHANNEL_CONF:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "GSM_RR_CHANNEL_CONF\n");
        md_input_netlink_radio_gsm_rr_channel_conf(min, obj);
        break;
    case RADIO_EVENT_CELL_LOCATION_GERAN:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "CELL_LOCATION_GERAN\n");
        md_input_netlink_radio_cell_loc_geran(min, obj);
        break;
    case RADIO_EVENT_GSM_RR_CELL_SEL_RESEL_PARAM:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "GSM_RR_CELL_SEL_RESEL_PARAM\n");
        md_input_netlink_radio_gsm_rr_cell_sel_reset_param(min, obj);
        break;
    case RADIO_EVENT_GRR_CELL_RESEL:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "GRR_CELL_RESEL\n");
        md_input_netlink_radio_grr_cell_resel(min, obj);
        break;
    case RADIO_EVENT_WCDMA_RRC_STATE:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "WCDMA_RRC_STATE\n");
        md_input_netlink_radio_wcdma_rrc_state(min, obj);
        break;
    case RADIO_EVENT_WCDMA_CELL_ID:
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "WCDMA_CELL_ID\n");
        md_input_netlink_radio_wcdma_cell_id(min, obj);
        break;

    default:
        break;
    }
}

static void md_input_netlink_handle_conn_event(struct md_input_netlink *min,
        struct json_object *obj)
{
    uint8_t retval = 0;

    init_conn_event(min->mce);
    retval = parse_conn_info(obj, min->mce, min->parent);

    if (retval == RETVAL_FAILURE)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) min->mce);
}

static void md_input_netlink_handle_gps_event(struct md_input_netlink *min,
                                              struct json_object *json_obj)
{
    struct md_gps_event* event = handle_gps_event(json_obj);

    if (!event)
        return;

    mde_publish_event_obj(min->parent, (struct md_event *) event);
    free(event);
}

static void md_input_netlink_handle_system_event(struct md_input_netlink *min,
        struct json_object *obj)
{
    //recycle iface event, it contains all fields we need (currently)
    memset(min->mse, 0, sizeof(md_system_event_t));
    min->mse->md_type = META_TYPE_SYSTEM;

    if (parse_iface_event(obj, min->mse, min->parent) == RETVAL_FAILURE)
        return;

    mde_publish_event_obj(min->parent, (struct md_event*) min->mse);
}

static void md_input_netlink_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_netlink *min = ptr;
    uint8_t rcv_buf[MNL_SOCKET_BUFFER_SIZE];
    ssize_t retval;
    struct nlmsghdr *nlh = (struct nlmsghdr*) rcv_buf;
    const char *nlh_payload;
    struct json_object *nlh_obj = NULL, *json_event = NULL;
    uint8_t event_type = 0;

    memset(rcv_buf, 0, sizeof(rcv_buf));

    //TODO: Consider adding support for fragmented data
    retval = mnl_socket_recvfrom(min->metadata_sock, rcv_buf,
            MNL_SOCKET_BUFFER_SIZE);

    if (retval <= 0)
        return;

    nlh_payload = mnl_nlmsg_get_payload(nlh);
    nlh_obj = json_tokener_parse(nlh_payload);

    if (!nlh_obj) {
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "Received invalid JSON object on Netlink socket\n");
        return;
    }

    //We are inserting version and sequence number. Version is so that the
    //application handling this data knows if it supports the format or not.
    //Sequence is so that we can see the order in which events arrived at the
    //metadata exporter, making it easier to correlate events between
    //applications. The different applications publishing data might also insert
    //their own sequence number
    if (add_json_key_value("md_seq", mde_inc_seq(min->parent), nlh_obj) ||
        add_json_key_value("md_ver", MDE_VERSION, nlh_obj)) {
        json_object_put(nlh_obj);
        return;
    }

    if (!json_object_object_get_ex(nlh_obj, "event_type", &json_event)) {
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "Missing event type\n");
        json_object_put(nlh_obj);
        return;
    }

    event_type = (uint8_t) json_object_get_int(json_event);

    if (!(event_type & min->md_nl_mask)) {
        json_object_put(nlh_obj);
        return;
    }

    META_PRINT(min->parent->logfile, "Got JSON %s\n", json_object_to_json_string(nlh_obj));

    switch (event_type) {
    case META_TYPE_INTERFACE:
        md_input_netlink_handle_iface_event(min, nlh_obj);
        break;
    case META_TYPE_CONNECTION:
        md_input_netlink_handle_conn_event(min, nlh_obj);
        break;
    case META_TYPE_POS:
        md_input_netlink_handle_gps_event(min, nlh_obj);
        break;
    case META_TYPE_RADIO:
        md_input_netlink_handle_radio_event(min, nlh_obj);
        break;
    case META_TYPE_SYSTEM:
        md_input_netlink_handle_system_event(min, nlh_obj);
        break;
    default:
        META_PRINT(min->parent->logfile, "Unknown event type\n");
        break;
    }

    json_object_put(nlh_obj);
}

static uint8_t md_input_netlink_config(struct md_input_netlink *min)
{

    //TODO: This code will be refactored to different importers, similar to
    //outputs
    min->metadata_sock = nlhelper_create_socket(NETLINK_USERSOCK,
            (1 << (METADATA_NL_GROUP - 1)));

    if (min->metadata_sock == NULL)
        return RETVAL_FAILURE;

    if(!(min->event_handle = backend_create_epoll_handle(min,
                    mnl_socket_get_fd(min->metadata_sock),
                    md_input_netlink_handle_event)))
        return RETVAL_FAILURE;

    backend_event_loop_update(min->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD,
        mnl_socket_get_fd(min->metadata_sock), min->event_handle);

    //TODO: guard with check for flag
    min->mce = calloc(sizeof(struct md_conn_event), 1);
    if (min->mce == NULL)
        return RETVAL_FAILURE;

    min->mie = calloc(sizeof(struct md_iface_event), 1);
    if (min->mie == NULL)
        return RETVAL_FAILURE;

    min->mre = calloc(sizeof(struct md_radio_event), 1);
    if (min->mre == NULL)
        return RETVAL_FAILURE;

    min->mse = calloc(sizeof(md_system_event_t), 1);
    if (min->mre == NULL)
        return RETVAL_FAILURE;


    return RETVAL_SUCCESS;
}

static uint8_t md_input_netlink_init(void *ptr, json_object* config)
{
    struct md_input_netlink *min = ptr;
    uint32_t md_nl_mask = 0;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "netlink", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "conn")) {
                md_nl_mask |= META_TYPE_CONNECTION;
            } else if (!strcmp(key, "pos")) {
                md_nl_mask |= META_TYPE_POS;
            } else if (!strcmp(key, "iface")) {
                md_nl_mask |= META_TYPE_INTERFACE;
            } else if (!strcmp(key, "radio")) {
                md_nl_mask |= META_TYPE_RADIO;
            } else if (!strcmp(key, "system")) {
                md_nl_mask |= META_TYPE_SYSTEM;
            }
        }
    }

    if (!md_nl_mask) {
        META_PRINT_SYSLOG(min->parent, LOG_ERR, "At least one netlink event type must be present\n");
        return RETVAL_FAILURE;
    }

    min->md_nl_mask = md_nl_mask;

    //Netlink has no arguments (for now), so just go right to config
    return md_input_netlink_config(min);
}

void md_netlink_usage()
{
    fprintf(stderr, "\"netlink\": {\t\tNetlink input (at least one event type must be present)\n");
    fprintf(stderr, "  \"conn\":\t\tReceive netlink connection events\n");
    fprintf(stderr, "  \"pos\":\t\tReceive netlink position events\n");
    fprintf(stderr, "  \"iface\":\t\tReceive netlink interface events\n");
    fprintf(stderr, "  \"radio\":\t\tReceive netlink radio events (QXDM + neigh. cells)\n");
    fprintf(stderr, "  \"system\":\t\tReceive netlink system (reboot) events\n");
    fprintf(stderr, "},\n");
}

void md_netlink_setup(struct md_exporter *mde, struct md_input_netlink *min)
{
    min->parent = mde;
    min->init = md_input_netlink_init;
}
