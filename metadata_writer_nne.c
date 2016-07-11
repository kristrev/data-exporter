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
#include "metadata_exporter_log.h"

#define NNE_DEFAULT_INTERVAL_MS 15000
#define NNE_DEFAULT_DIRECTORY "/nne/data/"
#define NNE_DEFAULT_GPS_PREFIX "gps_"
#define NNE_DEFAULT_GPS_EXTENSION ".sdat"
#define NNE_DEFAULT_METADATA_PREFIX "-metadatacollector-"
#define NNE_DEFAULT_METADATA_EXTENSION ".json"


struct nne_metadata_descr
{
    enum nne_metadata_idx idx;
    const char *key;
    enum nne_type type;
    int offset; // offset to member in md_iface_event structure
};

struct nne_metadata_descr NNE_METADATA_DESCR[] = {
    { NNE_IDX_MODE,    "mode",    NNE_TYPE_UINT8,  offsetof(struct md_iface_event, device_mode)    },
    { NNE_IDX_SUBMODE, "submode", NNE_TYPE_UINT8,  offsetof(struct md_iface_event, device_submode) },
    { NNE_IDX_RSSI,    "rssi",    NNE_TYPE_INT8,   offsetof(struct md_iface_event, rssi) },
    { NNE_IDX_RSCP,    "rscp",    NNE_TYPE_INT16,  offsetof(struct md_iface_event, rscp) },
    { NNE_IDX_ECIO,    "ecio",    NNE_TYPE_INT8,   offsetof(struct md_iface_event, ecio) },
    { NNE_IDX_RSRP,    "rsrp",    NNE_TYPE_INT16,  offsetof(struct md_iface_event, lte_rsrp) },
    { NNE_IDX_RSRQ,    "rsrq",    NNE_TYPE_INT8,   offsetof(struct md_iface_event, lte_rsrq) },
    { NNE_IDX_LAC,     "lac",     NNE_TYPE_UINT16, offsetof(struct md_iface_event, lac) },
    { NNE_IDX_CID,     "cid",     NNE_TYPE_INT32,  offsetof(struct md_iface_event, cid) },
    { NNE_IDX_OPER,    "oper",    NNE_TYPE_UINT32, offsetof(struct md_iface_event, nw_mccmnc) }
};

#define NNE_METADATA_DESCR_LEN (sizeof(NNE_METADATA_DESCR) / sizeof(struct nne_metadata_descr))

enum nne_message_type
{
    NNE_MESSAGE_TYPE_EVENT,
    NNE_MESSAGE_TYPE_BINS1MIN
};

enum nne_message_source
{
    NNE_MESSAGE_SOURCE_REPORT,
    NNE_MESSAGE_SOURCE_QUERY
};

struct nne_message
{
    enum nne_message_type  type;
    uint64_t tstamp;
    const char *node;
    uint32_t mccmnc;
    const char *key;
    struct nne_value value;
    const char* extra;
    enum nne_message_source source;
    uint64_t delta;
};

static uint8_t md_nne_handle_gps_event(struct md_writer_nne *mwn,
                                       struct md_gps_event *mge)
{
    char name_buf[256];
    char tm_buf[128];
    char xml_buf[128];
    struct tm *gtm;
    size_t retval;

    if (mge->minmea_id == MINMEA_SENTENCE_RMC)
        return RETVAL_IGNORE;

    if (mwn->gps_file == NULL) {
        if ((mwn->gps_file = fopen(mwn->gps_fname, "a")) == NULL) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to open file %s\n",
                    mwn->gps_fname);
            return RETVAL_FAILURE;
        }
    }

    if ((gtm = gmtime((time_t *)&mge->tstamp_tv.tv_sec)) == NULL) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Invalid GPS timestamp\n");
        return RETVAL_FAILURE;
    }

    if (!strftime(tm_buf, sizeof(tm_buf), "%Y-%m-%d %H:%M:%S", gtm)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format timestamp\n");
        return RETVAL_FAILURE;
    }

    retval = snprintf(xml_buf, sizeof(xml_buf),
            "<d e=\"0\"><lat>%f</lat><lon>%f</lon><speed>%f</speed></d>",
             mge->latitude, mge->longitude, mge->speed);
    if (retval >= sizeof(xml_buf)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format XML data\n");
        return RETVAL_FAILURE;
    }

    mwn->gps_sequence += 1;

    if (fprintf(mwn->gps_file, "%s.%06ld\t%i\t%i\t%s\n", tm_buf, mge->tstamp_tv.tv_usec,
            mwn->gps_instance_id, mwn->gps_sequence, xml_buf) < 0) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to write to export file%s\n",
                name_buf);
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

/*static void md_nne_handle_conn_event(struct md_writer_nne *mwn,
                                     struct md_conn_event* mce) {

    const char* const conn_event_type[] = {
        "",
        "CONN_EVENT_L3_UP",
        "CONN_EVENT_L3_DOWN",
        "CONN_EVENT_L4_UP",
        "CONN_EVENT_L4_DOWN",
        "CONN_EVENT_MODE_CHANGE",
        "CONN_EVENT_QUALITY_CHANGE",
        "CONN_EVENT_META_UPDATE",
        "CONN_EVENT_META_MODE_UPDATE",
        "CONN_EVENT_META_QUALITY_UPDATE",
    };

    META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: md_conn_event %s: interface_name=%s, imei=%s, network_address=%s, event_value=%d, event_value_str=%s",
                      conn_event_type[mce->event_param],  mce->interface_name, mce->imei, mce->network_address, mce->event_value, mce->event_value_str);
}*/

static struct nne_modem *md_nne_get_modem(struct nne_modem_list *modem_list, uint32_t mccmnc)
{
    struct nne_modem *e = NULL;
    for (e = modem_list->lh_first; e != NULL; e = e->entries.le_next)
        if (e->mccmnc == mccmnc)
            return e;

    return NULL;
}

struct nne_value nne_value_init(enum nne_type type, void *ptr, int offset)
{
    struct nne_value value;
    value.type = type;
    switch(type)
    {
        case NNE_TYPE_INT8:
            value.u.v_int8 = *(int8_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_UINT8:
            value.u.v_uint8 = *(uint8_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_INT16:
            value.u.v_int16 = *(int16_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_UINT16:
            value.u.v_uint16 = *(uint16_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_INT32:
            value.u.v_int32 = *(int32_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_UINT32:
            value.u.v_uint32 = *(uint32_t*)((char*)ptr + offset);
            break;
        case NNE_TYPE_STRING:
            value.u.v_str = (char*)ptr + offset;
            break;
    }

    return value;
}

static int nne_value_compare(struct nne_value v1, struct nne_value v2)
{
    if (v1.type != v2.type)
        return -2;

    switch(v1.type)
    {
        case NNE_TYPE_INT8:
            return (v1.u.v_int8 == v2.u.v_int8) ? 0 : ((v1.u.v_int8 < v2.u.v_int8) ? -1 : 1);
        case NNE_TYPE_UINT8:
            return (v1.u.v_uint8 == v2.u.v_uint8) ? 0 : ((v1.u.v_uint8 < v2.u.v_uint8) ? -1 : 1);
        case NNE_TYPE_INT16:
            return (v1.u.v_int16 == v2.u.v_int16) ? 0 : ((v1.u.v_int16 < v2.u.v_int16) ? -1 : 1);
        case NNE_TYPE_UINT16:
            return (v1.u.v_uint16 == v2.u.v_uint16) ? 0 : ((v1.u.v_uint16 < v2.u.v_uint16) ? -1 : 1);
        case NNE_TYPE_INT32:
            return (v1.u.v_int32 == v2.u.v_int32) ? 0 : ((v1.u.v_int32 < v2.u.v_int32) ? -1 : 1);
        case NNE_TYPE_UINT32:
            return (v1.u.v_uint32 == v2.u.v_uint32) ? 0 : ((v1.u.v_uint32 < v2.u.v_uint32) ? -1 : 1);
        case NNE_TYPE_STRING:
            return strcmp(v1.u.v_str, v2.u.v_str);
    }

    return -2;
}

static int md_nne_add_json_key_value_int(struct json_object *obj,
        const char *key, int64_t value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_int64(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_nne_add_json_key_value_string(struct json_object *obj,
        const char *key, const char *value)
{
    struct json_object *value_obj = NULL;

    value_obj = json_object_new_string(value);
    if (!value_obj)
        return RETVAL_FAILURE;

    json_object_object_add(obj, key, value_obj);
    return RETVAL_SUCCESS;
}

static int md_nne_add_json_key_value_null(struct json_object *obj,
        const char *key)
{
    json_object_object_add(obj, key, NULL);
    return RETVAL_SUCCESS;
}

static int md_nne_add_json_key_value(struct json_object *obj,
        const char* key, struct nne_value value)
{
    switch(value.type)
    {
        case NNE_TYPE_INT8:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_int8);
        case NNE_TYPE_UINT8:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_uint8);
        case NNE_TYPE_INT16:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_int16);
        case NNE_TYPE_UINT16:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_uint16);
        case NNE_TYPE_INT32:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_int32);
        case NNE_TYPE_UINT32:
            return md_nne_add_json_key_value_int(obj, key, value.u.v_uint32);
        case NNE_TYPE_STRING:
            return md_nne_add_json_key_value_string(obj, key, value.u.v_str);
    }
    return RETVAL_FAILURE;
}

static void md_nne_send_message(struct md_writer_nne *mwn,
        struct nne_message* msg)
{
    static const char *NNE_MESSAGE_TYPE_STR[] = {
        "event",
        "bins-1min"
    };

    static const char *NNE_MESSAGE_SOURCE_STR[] = {
        "report",
        "query"
    };

    struct json_object* obj = json_object_new_object();

    md_nne_add_json_key_value_string(obj, "type", NNE_MESSAGE_TYPE_STR[msg->type]);
    md_nne_add_json_key_value_int(obj, "ts", msg->tstamp);
    md_nne_add_json_key_value_string(obj, "node", msg->node);
    md_nne_add_json_key_value_int(obj, "mccmnc", msg->mccmnc);
    md_nne_add_json_key_value_string(obj, "key", msg->key);

    md_nne_add_json_key_value(obj, "value", msg->value);

    if (msg->extra == NULL)
        md_nne_add_json_key_value_null(obj, "extra");
    else
        md_nne_add_json_key_value_string(obj, "extra", msg->extra);

    md_nne_add_json_key_value_string(obj, "source", NNE_MESSAGE_SOURCE_STR[msg->source]);

    if (msg->type == NNE_MESSAGE_TYPE_BINS1MIN)
        md_nne_add_json_key_value_int(obj, "delta", msg->delta);

    META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: %s\n", msg->mccmnc,
                      json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));

    if (mwn->metadata_cache == NULL)
        mwn->metadata_cache = json_object_new_array();

    json_object_array_add(mwn->metadata_cache, obj);

    //json_object_put(obj);
}

static void md_nne_process_iface_event(struct md_writer_nne *mwn,
                                       struct nne_metadata_descr* descr,
                                       struct nne_modem* modem,
                                       struct md_iface_event *mie,
                                       enum nne_message_source source)
{
    struct nne_value value = nne_value_init(descr->type, mie, descr->offset);

    if (nne_value_compare(modem->metadata[descr->idx].value, value) != 0)
    {
        modem->metadata[descr->idx].value = value;
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: %d,%s changed %d\n",
                          mie->imsi_mccmnc, descr->idx, descr->key,
                          modem->metadata[descr->idx].value.u.v_uint8);

        // generate json event
        struct nne_message msg;
        msg.type = NNE_MESSAGE_TYPE_EVENT;
        msg.tstamp = mie->tstamp;
        msg.node = mwn->node_id;
        msg.mccmnc = mie->imsi_mccmnc;
        msg.key = descr->key;
        msg.value = value;
        msg.extra = NULL;
        msg.source = source;
        msg.delta = 0;

        md_nne_send_message(mwn, &msg);

    }

    modem->metadata[descr->idx].tstamp = mie->tstamp;

    if (source == NNE_MESSAGE_SOURCE_QUERY)
    {
        // generate json event
        struct nne_message msg;
        msg.type = NNE_MESSAGE_TYPE_BINS1MIN;
        msg.tstamp = (mie->tstamp / 60) * 60;
        msg.node = mwn->node_id;
        msg.mccmnc = mie->imsi_mccmnc;
        msg.key = descr->key;
        msg.value = value;
        msg.extra = NULL;
        msg.source = source;
        msg.delta = 0;

        md_nne_send_message(mwn, &msg);
    }
}

static void md_nne_handle_iface_event(struct md_writer_nne *mwn,
                                      struct md_iface_event *mie)
{

    const char* const iface_event_type[] = {
        "",
        "IFACE_EVENT_DEV_STATE",
        "IFACE_EVENT_MODE_CHANGE",
        "IFACE_EVENT_SIGNAL_CHANGE",
        "IFACE_EVENT_LTE_BAND_CHANGE",
        "IFACE_EVENT_ISP_NAME_CHANGE",
        "IFACE_EVENT_UPDATE",
        "IFACE_EVENT_IP_ADDR_CHANGE",
        "IFACE_EVENT_LOC_CHANGE",
        "IFACE_EVENT_NW_MCCMNC_CHANGE",
    };

    META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: %s: "
                      "ip_addr=%s, "
                      "ifname=%s, "
                      "imsi_mccmnc=%d, "
                      "nw_mccmnc=%d, "
                      "cid=%d, "
                      "rscp=%d, "
                      "lte_rsrp=%d, "
                      "lac=%d, "
                      "rssi=%d, "
                      "ecio=%d, "
                      "lte_rssi=%d, "
                      "lte_rsrq=%d, "
                      "device_mode=%d, "
                      "device_submode=%d\n",
                      iface_event_type[mie->event_param],
                      mie->ip_addr,
                      mie->ifname,
                      mie->imsi_mccmnc,
                      mie->nw_mccmnc,
                      mie->cid,
                      mie->rscp,
                      mie->lte_rsrp,
                      mie->lac,
                      mie->rssi,
                      mie->ecio,
                      mie->lte_rssi,
                      mie->lte_rsrq,
                      mie->device_mode,
                      mie->device_submode);


    struct nne_modem *modem = NULL;
    enum nne_message_source source;
    int i;


    modem = md_nne_get_modem(&(mwn->modem_list), mie->imsi_mccmnc);

    if (modem == NULL)
    {
//        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: No modem found\n", mie->imsi_mccmnc);
        // Create new modem

        modem = malloc(sizeof(struct nne_modem));
        LIST_INSERT_HEAD(&(mwn->modem_list), modem, entries);
        modem->tstamp = mie->tstamp;
        modem->mccmnc = mie->imsi_mccmnc;
//        memcpy(modem->metadata, NNE_INITIAL_METADATA, sizeof(NNE_INITIAL_METADATA));
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: Added new  modem\n", mie->imsi_mccmnc);
        // generate usbmodem UP event
    }

    META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: modem %d, sec=%ld\n",
                      mie->imsi_mccmnc, modem->mccmnc, mie->tstamp % 60);

    source = NNE_MESSAGE_SOURCE_REPORT;
    if (mie->event_param == IFACE_EVENT_UPDATE && mie->tstamp % 60 < 30)
    {
        source = NNE_MESSAGE_SOURCE_QUERY;
    }

    for (i = 0; i < NNE_METADATA_DESCR_LEN; i++)
    {
        md_nne_process_iface_event(mwn, &(NNE_METADATA_DESCR[i]), modem, mie, source);
    }
/*

    // update modem's metadata
    if (mie->device_mode != modem->metadata[NNE_IDX_MODE].value.v_uint8)
    {
        modem->metadata[NNE_IDX_MODE].tstamp = mie->tstamp;
        modem->metadata[NNE_IDX_MODE].value.v_uint8 = mie->device_mode;
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: mode changed %d\n", mie->imsi_mccmnc, mie->device_mode);
        // append event to json file


        struct json_object* obj = json_object_new_object();

        struct json_object* obj_add = json_object_new_int(mie->imsi_mccmnc);
//        if (obj_add == NULL) {
//            json_object_put(obj);
//            return NULL;
//        }
        json_object_object_add(obj, "mccmnc", obj_add);


        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: mccmnc %d: %s\n", mie->imsi_mccmnc, json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));


        json_object_put(obj);

    }
    else
    {
        modem->metadata[NNE_IDX_MODE].tstamp = mie->tstamp;
    }
*/
    // TODO Go through all modems, find stale ones (tstamp > 60 secs),
    // generate usbmodem LOST event and remove the modem from the list

/*
    if (mie->mie->device_mode != mwn->metadata[NNE_IDX_MODE].value.v_int)
    {
        mwn->metadata[NNE_IDX_MODE].tstamp = tstamp;
        mwn->metadata[NNE_IDX_MODE].value = mie->mie->device_mode;
        // append event to json file
    }
    else
    {
        mwn->metadata[NNE_IDX_MODE].tstamp = tstamp;
    }
*/
}

static int md_nne_format_fname_tm(char *s, int max, const char *format)
{
    struct timeval tv;
    struct tm *gtm;
    char tm_buf[128];
    size_t retval;

    if (gettimeofday(&tv, NULL)) {
        return RETVAL_FAILURE;
    }

    if ((gtm = gmtime((time_t *)&tv.tv_sec)) == NULL) {
        return RETVAL_FAILURE;
    }

    if (!strftime(tm_buf, sizeof(tm_buf), "%Y%m%dT%H%M%S", gtm)) {
        return RETVAL_FAILURE;
    }

    retval = snprintf(s, max, format, tm_buf);
    if (retval >= max) {
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

static void md_nne_handle_gps_timeout(struct md_writer_nne *mwn)
{
    char fname_tm[128];

    if (mwn->gps_file != NULL) {
        fclose(mwn->gps_file);
        mwn->gps_file = NULL;

        if (md_nne_format_fname_tm(fname_tm, sizeof(fname_tm), mwn->gps_fname_tm)) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
            return;
        }
        META_PRINT_SYSLOG(mwn->parent, LOG_INFO, "NNE writer: fname_tm: %s\n", fname_tm);

        if (rename(mwn->gps_fname, fname_tm)) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to rename file %s to %s\n",
                              mwn->gps_fname, fname_tm);
            return;
        }

        META_PRINT_SYSLOG(mwn->parent, LOG_INFO, "NNE writer: Rotated file %s to %s\n",
                          mwn->gps_fname, fname_tm);
    }
}

static void md_nne_handle_metadata_timeout(struct md_writer_nne *mwn)
{
    FILE *file;
    const char* str;
    char fname_tm[128];

    if (mwn->metadata_cache != NULL)
    {
        if ((file = fopen(mwn->metadata_fname, "w")) == NULL) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to open file %s\n",
                    mwn->metadata_fname);
            return;
        }

        str = json_object_to_json_string_ext(mwn->metadata_cache, JSON_C_TO_STRING_PLAIN);

        if (fprintf(file, "%s", str) < 0) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to write to file %s\n",
                    mwn->metadata_fname);
            return;
        }

        fclose(file);

        json_object_put(mwn->metadata_cache);
        mwn->metadata_cache = NULL;

        if (md_nne_format_fname_tm(fname_tm, sizeof(fname_tm), mwn->metadata_fname_tm)) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
            return;
        }

        if (rename(mwn->metadata_fname, fname_tm)) {
            META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to rename file %s to %s\n",
                mwn->metadata_fname, fname_tm);
            return;
        }

        META_PRINT_SYSLOG(mwn->parent, LOG_INFO, "NNE writer: Metadata exported to file %s\n",
                fname_tm);
    }
}

static void md_nne_handle_timeout(void *ptr)
{
    struct md_writer_nne *mwn = ptr;
    md_nne_handle_gps_timeout(mwn);
    md_nne_handle_metadata_timeout(mwn);
}

static int32_t md_nne_init(void *ptr, json_object* config)
{
    struct md_writer_nne *mwn = ptr;
    size_t retval;

    strcpy(mwn->directory, NNE_DEFAULT_DIRECTORY);
    mwn->interval = NNE_DEFAULT_INTERVAL_MS;
    strcpy(mwn->node_id, "");
    strcpy(mwn->gps_prefix, NNE_DEFAULT_GPS_PREFIX);
    mwn->gps_instance_id = 0;
    strcpy(mwn->gps_extension, NNE_DEFAULT_GPS_EXTENSION);
    strcpy(mwn->metadata_prefix, NNE_DEFAULT_METADATA_PREFIX);
    strcpy(mwn->metadata_extension, NNE_DEFAULT_METADATA_EXTENSION);

    mwn->gps_file = NULL;
    mwn->gps_sequence = 0;

    mwn->metadata_cache = NULL;
    LIST_INIT(&(mwn->modem_list));

    json_object* subconfig;
    if (json_object_object_get_ex(config, "nne", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "node_id")) {
                const char* node_id = json_object_get_string(val);
                if (strlen(node_id) > sizeof(mwn->node_id) - 1) {
                    META_PRINT_SYSLOG(mwn->parent, LOG_ERR,
                            "NNE writer: node_id too long (>%ld)\n",
                            sizeof(mwn->node_id) - 1);
                    return RETVAL_FAILURE;
                } else
                    strcpy(mwn->node_id, node_id);
            }
            else if (!strcmp(key, "interval"))
                mwn->interval = ((uint32_t) json_object_get_int(val)) * 1000;
            else if (!strcmp(key, "gps_instance"))
                mwn->gps_instance_id = ((uint32_t) json_object_get_int(val)) * 1000;
            else if (!strcmp(key, "gps_prefix")) {
                const char* prefix = json_object_get_string(val);
                if (strlen(prefix) > sizeof(mwn->gps_prefix) - 1) {
                    META_PRINT_SYSLOG(mwn->parent, LOG_ERR,
                            "NNE writer: gps prefix too long (>%ld)\n",
                            sizeof(mwn->gps_prefix) - 1);
                    return RETVAL_FAILURE;
                } else
                    strcpy(mwn->gps_prefix, prefix);
            }
        }
    }

    if (strlen(mwn->node_id) == 0) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Missing mandatory parameter node_id\n");
        return RETVAL_FAILURE;
    }

    retval = snprintf(mwn->gps_fname, sizeof(mwn->gps_fname), "%s%s%d%s",
                      mwn->directory, mwn->gps_prefix,
                      mwn->gps_instance_id, mwn->gps_extension);
    if (retval >= sizeof(mwn->gps_fname)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
        return RETVAL_FAILURE;
    }

    retval = snprintf(mwn->gps_fname_tm, sizeof(mwn->gps_fname_tm), "%s%s%d%s.%%s",
                      mwn->directory, mwn->gps_prefix,
                      mwn->gps_instance_id, mwn->gps_extension);
    if (retval >= sizeof(mwn->gps_fname_tm)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
        return RETVAL_FAILURE;
    }

    retval = snprintf(mwn->metadata_fname, sizeof(mwn->metadata_fname), "%s.%s%stmp%s",
                      mwn->directory, mwn->node_id,
                      mwn->metadata_prefix, mwn->metadata_extension);
    if (retval >= sizeof(mwn->metadata_fname)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
        return RETVAL_FAILURE;
    }

    retval = snprintf(mwn->metadata_fname_tm, sizeof(mwn->metadata_fname_tm), "%s%s%s%%s%s",
                      mwn->directory, mwn->node_id,
                      mwn->metadata_prefix, mwn->metadata_extension);
    if (retval >= sizeof(mwn->metadata_fname_tm)) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to format file name\n");
        return RETVAL_FAILURE;
    }

    if(!(mwn->timeout_handle = backend_event_loop_create_timeout(0,
             md_nne_handle_timeout, mwn, mwn->interval))) {
        META_PRINT_SYSLOG(mwn->parent, LOG_ERR, "NNE writer: Failed to create file rotation/export timeout\n");
        return RETVAL_FAILURE;
    }

    mde_start_timer(mwn->parent->event_loop, mwn->timeout_handle,
                    mwn->interval);

    return RETVAL_SUCCESS;
}

static void md_nne_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_nne *mwn = (struct md_writer_nne*) writer;

    switch (event->md_type) {
        case META_TYPE_POS:
            md_nne_handle_gps_event(mwn, (struct md_gps_event*) event);
            break;
//        case META_TYPE_CONNECTION:
//            md_nne_handle_conn_event(mwn, (struct md_conn_event*) event);
//            break;
        case META_TYPE_INTERFACE:
            md_nne_handle_iface_event(mwn, (struct md_iface_event*) event);
            break;
        default:
            return;
        }
}

void md_nne_usage()
{
    fprintf(stderr, "\"nne\": {\t\tNornet Edge writer.\n");
    fprintf(stderr, "  \"node_id\":\t\tNornet node id (e.g. nne601)\n");
    fprintf(stderr, "  \"interval\":\t\tFile rotation/export interval (in seconds)\n");
    fprintf(stderr, "  \"gsp_instance\":\t\tNNE measurement instance id for gps\n");
    fprintf(stderr, "  \"gps_prefix\":\t\tFile prefix for gps /nne/data/<PREFIX><INSTANCE>.sdat\n");
    fprintf(stderr, "},\n");
}

void md_nne_setup(struct md_exporter *mde, struct md_writer_nne* mwn)
{
    mwn->parent = mde;
    mwn->init = md_nne_init;
    mwn->handle = md_nne_handle;
}

