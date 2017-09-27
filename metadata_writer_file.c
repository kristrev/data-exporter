/* Copyright (c) 2017, Celerway, Lukasz Baj <l.baj@radytek.com>
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
#include <stdlib.h>
#include <unistd.h>

#include "backend_event_loop.h"
#include "metadata_exporter_log.h"
#include "metadata_exporter.h"
#include "metadata_writer_file.h"

static int md_file_add_json_int(struct json_object *obj,
        const char *key, int64_t value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_int64(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_file_add_json_string(struct json_object *obj,
        const char *key, const char *value)
{
    struct json_object *value_obj = NULL;

    if (value != NULL)
    {
        value_obj = json_object_new_string(value);
        if (!value_obj)
            return RETVAL_FAILURE;
    }

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_file_save(struct md_writer_file *mwf, int event_type, const char *content)
{
    int output_fd;
    char dst_filename[256];
    FILE *output;

    memset(mwf->prefix + mwf->prefix_len, 'X', 6);

    output_fd = mkstemp(mwf->prefix);
    if (output_fd == -1) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "Could not create temporary filename. Error: %s\n", strerror(errno));
        return RETVAL_FAILURE;
    }

    output = fdopen(output_fd, "w");

    if (!output) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "Could not open random file as FILE*. Error: %s\n", strerror(errno));
        remove(mwf->prefix);
        return RETVAL_FAILURE;
    }

    fprintf(output, "%s", content);
    fclose(output);

    snprintf(dst_filename, 256, "%s_%d.json", mwf->prefix, event_type);
    META_PRINT_SYSLOG(mwf->parent, LOG_INFO, "Done with tmpfile %s\n", dst_filename);

    if (link(mwf->prefix, dst_filename) || unlink(mwf->prefix)) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "Could not link/unlink dump-file: %s\n", strerror(errno));
        remove(mwf->prefix);
        remove(dst_filename);
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

static void md_file_handle_iface_event(struct md_writer_file *mwf,
                                      struct md_iface_event *mie)
{
    const char *json_string;
    struct json_object *obj = json_object_new_object();

    if (obj == NULL) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_iface_event: Can't allocate iface json object!");
        return;
    }

    if (md_file_add_json_int(obj, "timestamp", mie->tstamp) ||
        md_file_add_json_int(obj, "event_type", mie->md_type) ||
        md_file_add_json_int(obj, "event_param", mie->event_param) ||
        md_file_add_json_int(obj, "sequence", mie->sequence) ||
        md_file_add_json_string(obj, "iccid", mie->iccid) ||
        md_file_add_json_string(obj, "imsi", mie->imsi) ||
        md_file_add_json_string(obj, "imei", mie->imei) ||
        md_file_add_json_string(obj, "isp_name", mie->isp_name) ||
        md_file_add_json_int(obj, "mode", mie->device_mode) ||
        md_file_add_json_int(obj, "submode", mie->device_submode) ||
        md_file_add_json_int(obj, "cid", mie->cid) ||
        md_file_add_json_int(obj, "enodeb_id", mie->enodeb_id) ||
        md_file_add_json_int(obj, "lac", mie->lac) ||
        md_file_add_json_int(obj, "device_state", mie->device_state)) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_iface_event: Can't create iface values to object!");
        json_object_put(obj);
        return;
    }

    if (mie->device_mode != 5) {
        if (md_file_add_json_int(obj, "rssi", mie->rssi) ||
            md_file_add_json_int(obj, "rscp", mie->rscp) ||
            md_file_add_json_int(obj, "ecio", mie->ecio)) {
            META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_iface_event: Can't add non-LTE values to object!");
            json_object_put(obj);
            return;
        }
    } else { // LTE
        if (md_file_add_json_int(obj, "lte_rssi", mie->lte_rssi) ||
            md_file_add_json_int(obj, "lte_rsrp", mie->lte_rsrp) ||
            md_file_add_json_int(obj, "lte_rsrq", mie->lte_rsrq) ||
            md_file_add_json_int(obj, "lte_freq", mie->lte_freq) ||
            md_file_add_json_int(obj, "lte_pci", mie->lte_pci) ||
            md_file_add_json_int(obj, "lte_band", mie->lte_band)) {
            META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_iface_event: Can't add LTE values to object!");
            json_object_put(obj);
            return;
        }
    }

    json_string = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    md_file_save(mwf, mie->event_type, json_string);

    json_object_put(obj);
}

static void md_file_handle_conn_event(struct md_writer_file *mwf,
                                      struct md_conn_event *mce)
{
    const char *json_string;
    struct json_object *obj = json_object_new_object();

    if (obj == NULL) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_conn_event: Can't allocate iface json object!");
        return;
    }

    if (md_file_add_json_int(obj, "timestamp", mce->tstamp) ||
        md_file_add_json_int(obj, "event_type", mce->md_type) ||
        md_file_add_json_int(obj, "event_param", mce->event_param) ||
        md_file_add_json_int(obj, "event_value", mce->event_value) ||
        md_file_add_json_int(obj, "sequence", mce->sequence) ||
        md_file_add_json_string(obj, "imei", mce->imei) ||
        md_file_add_json_string(obj, "imsi", mce->imsi) ||
        md_file_add_json_int(obj, "interface_type", mce->interface_type) ||
        md_file_add_json_int(obj, "network_address_family", mce->network_address_family) ||
        md_file_add_json_int(obj, "network_provider_type", mce->network_provider_type) ||
        md_file_add_json_int(obj, "signal_strength", mce->signal_strength) ||
        md_file_add_json_int(obj, "l3_session_id", mce->l3_session_id) ||
        md_file_add_json_int(obj, "l4_session_id", mce->l4_session_id) ||
        md_file_add_json_int(obj, "rx_bytes", mce->rx_bytes) ||
        md_file_add_json_int(obj, "tx_bytes", mce->tx_bytes) ||
        md_file_add_json_string(obj, "interface_id", mce->interface_id) ||
        md_file_add_json_string(obj, "interface_name", mce->interface_name) ||
        md_file_add_json_int(obj, "network_provider", mce->network_provider) ||
        md_file_add_json_string(obj, "network_address", mce->network_address) ||
        md_file_add_json_int(obj, "connectivity", mce->connectivity) ||
        md_file_add_json_int(obj, "connection_mode", mce->connection_mode) ||
        md_file_add_json_int(obj, "quality", mce->quality)) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_conn_event: Can't add conn values to object!");
        json_object_put(obj);
        return;
    }

    json_string = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    md_file_save(mwf, mce->event_type, json_string);

    json_object_put(obj);
}

static const char* md_file_convert_float_to_string(const float number)
{
    static char text[32];

    memset(text, 0, sizeof(text));
    sprintf(text, "%f", number);
    return text;
}

static void md_file_handle_gps_event(struct md_writer_file *mwf,
                                      struct md_gps_event *mge)
{
    const char *json_string;
    struct json_object *obj = json_object_new_object();

    if (obj == NULL) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_gps_event: Can't allocate iface json object!");
        return;
    }

    if (md_file_add_json_int(obj, "timestamp", mge->tstamp) ||
        md_file_add_json_int(obj, "event_type", mge->md_type) ||
        md_file_add_json_int(obj, "sequence", mge->sequence) ||
        md_file_add_json_int(obj, "tstamp_tv.tv_sec", mge->tstamp_tv.tv_sec) ||
        md_file_add_json_int(obj, "tstamp_tv.tv_usec", mge->tstamp_tv.tv_usec) ||
        md_file_add_json_string(obj, "nmea_raw", mge->nmea_raw) ||
        md_file_add_json_int(obj, "time.hours", mge->time.hours) ||
        md_file_add_json_int(obj, "time.minutes", mge->time.minutes) ||
        md_file_add_json_int(obj, "time.seconds", mge->time.seconds) ||
        md_file_add_json_int(obj, "time.microseconds", mge->time.microseconds) ||
        md_file_add_json_string(obj, "latitude", md_file_convert_float_to_string(mge->latitude)) ||
        md_file_add_json_string(obj, "longitude", md_file_convert_float_to_string(mge->longitude)) ||
        md_file_add_json_string(obj, "speed", md_file_convert_float_to_string(mge->speed)) ||
        md_file_add_json_string(obj, "altitude", md_file_convert_float_to_string(mge->altitude)) ||
        md_file_add_json_int(obj, "time.microseconds", mge->satellites_tracked) ||
        md_file_add_json_int(obj, "time.microseconds", mge->minmea_id)) {
        META_PRINT_SYSLOG(mwf->parent, LOG_ERR, "md_file_handle_gps_event: Can't add gps values to object!");
        json_object_put(obj);
        return;
    }

    json_string = json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
    md_file_save(mwf, mge->md_type, json_string);

    json_object_put(obj);
}

static int32_t md_file_init(void *ptr, json_object* config)
{
    struct md_writer_file *mwf = ptr;
    memset(mwf->prefix, 0, 128);

    json_object* subconfig;
    if (json_object_object_get_ex(config, "file", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "prefix") && json_object_is_type(val, json_type_string)) {
                strncpy(mwf->prefix, json_object_get_string(val), json_object_get_string_len(val));
                mwf->prefix_len = strlen(mwf->prefix);
                return RETVAL_SUCCESS;
            }
        }
    }

    META_PRINT_SYSLOG(mwf->parent, LOG_INFO, "md_file_init: Can't initialize writer!");
    return RETVAL_FAILURE;
}

static void md_file_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_file *mwf = (struct md_writer_file*) writer;
    META_PRINT_SYSLOG(mwf->parent, LOG_INFO, "md_file_handle: event type %u\n", event->md_type);

    switch (event->md_type) {
        case META_TYPE_INTERFACE:
            md_file_handle_iface_event(mwf, (struct md_iface_event*) event);
            break;
        case META_TYPE_CONNECTION:
            md_file_handle_conn_event(mwf, (struct md_conn_event*) event);
            break;
        case META_TYPE_POS:
            md_file_handle_gps_event(mwf, (struct md_gps_event*) event);
            break;

    default:
        return;
    }
}

void md_file_usage()
{
    fprintf(stderr, "\"file\": {\t\tFile writer.\n");
    fprintf(stderr, "  \"prefix\":\tOutput file name prefix\n");
    fprintf(stderr, "}\n");
}

void md_file_setup(struct md_exporter *mde, struct md_writer_file *mwf)
{
    mwf->parent = mde;
    mwf->init = md_file_init;
    mwf->handle = md_file_handle;
}
