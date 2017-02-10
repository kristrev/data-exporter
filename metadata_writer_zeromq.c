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
#include <string.h>
#include <zmq.h>
#include JSON_LOC
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <getopt.h>

#include "lib/minmea.h"
#include "metadata_exporter.h"
#include "metadata_writer_zeromq.h"
#include "system_helpers.h"
#include "metadata_utils.h"
#include "metadata_exporter_log.h"

static void md_zeromq_add_default_fields(json_object* obj, int seq, int64_t tstamp, char* dataid) {
    json_object* obj_add = NULL;

    if (!(obj_add = json_object_new_int(seq))) return;
    json_object_object_add(obj, ZMQ_KEY_SEQ, obj_add);

    if (!(obj_add = json_object_new_int64(tstamp))) return;
    json_object_object_add(obj, ZMQ_KEY_TSTAMP, obj_add);

#ifdef MONROE
    if (!(obj_add = json_object_new_int(MONROE_ZMQ_DATA_VERSION))) return;
    json_object_object_add(obj, ZMQ_KEY_DATAVERSION, obj_add);

    if (!(obj_add = json_object_new_string(dataid))) return;
    json_object_object_add(obj, ZMQ_KEY_DATAID, obj_add);
#endif
}

static json_object *md_zeromq_create_json_string(json_object *obj,
        const char *key, const char *value)
{
    if (value == NULL) return NULL;

    struct json_object *obj_add = json_object_new_string(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object *md_zeromq_create_json_int(json_object *obj, const char *key,
        int value)
{
    struct json_object *obj_add = json_object_new_int(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object *md_zeromq_create_json_int64(json_object *obj,
        const char *key, int64_t value)
{
    struct json_object *obj_add = json_object_new_int64(value);

    if (!obj_add)
        return NULL;

    json_object_object_add(obj, key, obj_add);
    return obj;
}

static json_object* md_zeromq_create_json_gps(struct md_writer_zeromq *mwz,
                                              struct md_gps_event *mge)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    char topic[256];
    strcat(topic, MONROE_ZMQ_DATA_ID_GPS);
    if (mge->nmea_raw) {
    	if (strncmp(mge->nmea_raw, "GPGGA", 5)==0) {
    	    strcat(topic, ".GPGGA");
    	} else if (strncmp(mge->nmea_raw, "GPRMC", 5)==0) {
            strcat(topic, ".GPRMC");
        }	
    } 
    md_zeromq_add_default_fields(obj, mge->sequence, mge->tstamp_tv.tv_sec, topic);

    if (!(obj_add = json_object_new_double(mge->latitude))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, ZMQ_KEY_LATITUDE, obj_add);

    if (!(obj_add = json_object_new_double(mge->longitude))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, ZMQ_KEY_LONGITUDE, obj_add);

    if (mge->speed) {
        if (isnan(mge->speed)) {
            json_object_object_add(obj, ZMQ_KEY_SPEED, NULL);
        } else {
            obj_add = json_object_new_double(mge->speed);

            if (obj_add == NULL) {
                json_object_put(obj);
                return NULL;
            }
            json_object_object_add(obj, ZMQ_KEY_SPEED, obj_add);
        }
    }

    if (mge->altitude) {
        obj_add = json_object_new_double(mge->altitude);

        if (obj_add == NULL) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, ZMQ_KEY_ALTITUDE, obj_add);
    }

    if (mge->satellites_tracked) {
        obj_add = json_object_new_int(mge->satellites_tracked);

        if (obj_add == NULL) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, ZMQ_KEY_NUMSAT, obj_add);
    }

    if (mge->nmea_raw) {
        if (!(obj_add = json_object_new_string(mge->nmea_raw))) {
            json_object_put(obj);
            return NULL;
        }
        json_object_object_add(obj, ZMQ_KEY_NMEA, obj_add);
    }

    return obj;
}

static void md_zeromq_send(struct md_writer_zeromq* mwz, const void *buf, size_t len, int flags) {
    if (mwz->connected != 1) {
        if ((zmq_bind(mwz->zmq_publisher, mwz->zmq_addr)) == 0) {
            mwz->connected = 1;
        }
    }
    if (mwz->connected == 1) {
        if (zmq_send(mwz->zmq_publisher, buf, len, flags) != 0) {
            //META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "zmq_send returned errno %s (dropped message of len %zu)\n",
            //    zmq_strerror(errno), len);
            //META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "Message: %.128s(...)\n", (char *)buf);
            if (errno != EAGAIN ) {
                zmq_unbind(mwz->zmq_publisher, mwz->zmq_addr);
                mwz->connected = 0;
            }
        }
    }
}

static void md_zeromq_handle_gps(struct md_writer_zeromq *mwz,
                                 struct md_gps_event *mge)
{
    char topic[8192];
    struct json_object *gps_obj = md_zeromq_create_json_gps(mwz, mge);
    int retval;

    if (gps_obj == NULL) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to create GPS ZMQ JSON\n");
        return;
    }

    retval = snprintf(topic, sizeof(topic), "%s %s", MONROE_ZMQ_TOPIC_GPS, json_object_to_json_string_ext(gps_obj, JSON_C_TO_STRING_PLAIN));

    if (retval < sizeof(topic)) {
        md_zeromq_send(mwz, topic, strlen(topic), 0);
    }
    json_object_put(gps_obj);
}

static void md_zeromq_handle_munin(struct md_writer_zeromq *mwz,
                                   struct md_munin_event *mge)
{
    char topic[8192];

    int retval;

    json_object_object_foreach(mge->json_blob, key, val) {
        md_zeromq_add_default_fields(val, mge->sequence, mge->tstamp, MONROE_ZMQ_DATA_ID_SENSOR);

        retval = snprintf(topic, sizeof(topic), "%s.%s %s", MONROE_ZMQ_TOPIC_SENSOR, key, json_object_to_json_string_ext(val, JSON_C_TO_STRING_PLAIN));
        if (retval < sizeof(topic)) {
            md_zeromq_send(mwz, topic, strlen(topic), 0);
        }
    }
}


static void md_zeromq_handle_sysevent(struct md_writer_zeromq *mwz,
                                   struct md_sysevent *mge)
{
    char topic[8192];
    int retval;

    md_zeromq_add_default_fields(mge->json_blob, mge->sequence, mge->tstamp, MONROE_ZMQ_DATA_ID_SYSEVENT);
    retval = snprintf(topic, sizeof(topic), "%s %s", MONROE_ZMQ_TOPIC_SYSEVENT, json_object_to_json_string_ext(mge->json_blob, JSON_C_TO_STRING_PLAIN));
    if (retval < sizeof(topic)) {
        md_zeromq_send(mwz, topic, strlen(topic), 0);
    }
}


static json_object* md_zeromq_create_json_modem_default(struct md_writer_zeromq *mwz,
                                                        struct md_conn_event *mce)
{
    struct json_object *obj = NULL, *obj_add = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    md_zeromq_add_default_fields(obj, mce->sequence, mce->tstamp, MONROE_ZMQ_DATA_ID_CONNECTIVITY);

    if (!(obj_add = json_object_new_string(mce->interface_id))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, ZMQ_KEY_INTERFACEID, obj_add);

    if (!(obj_add = json_object_new_string(mce->interface_name))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, ZMQ_KEY_INTERFACENAME, obj_add);

    if (!(obj_add = json_object_new_int(mce->network_provider))) {
        json_object_put(obj);
        return NULL;
    }
    json_object_object_add(obj, ZMQ_KEY_OPERATOR, obj_add);

    return obj;
}

static void md_zeromq_handle_conn(struct md_writer_zeromq *mwz,
                                   struct md_conn_event *mce)
{
    struct json_object *json_obj, *obj_add;
    char event_str_cpy[EVENT_STR_LEN];
    size_t event_str_len;
    uint8_t mode;
    char topic[8192];
    int retval;

    if ((mce->event_param != CONN_EVENT_MODE_CHANGE &&
         mce->event_param != CONN_EVENT_META_UPDATE) ||
         mce->interface_type != INTERFACE_MODEM)
        return;

    if (mce->event_param == CONN_EVENT_MODE_CHANGE) {
        mode = mce->event_value;
    } else {
        event_str_len = strlen(mce->event_value_str);

        if (event_str_len >= EVENT_STR_LEN) {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Event string too long\n");
            return;
        }

        memcpy(event_str_cpy, mce->event_value_str, event_str_len);
        event_str_cpy[event_str_len] = '\0';
        mode = metadata_utils_get_csv_pos(event_str_cpy, 2);
    }

    json_obj = md_zeromq_create_json_modem_default(mwz, mce);

    if (!(obj_add = json_object_new_int(mode))) {
        json_object_put(json_obj);
        return;
    }
    json_object_object_add(json_obj, ZMQ_KEY_MODE, obj_add);

    if (mce->event_param == CONN_EVENT_META_UPDATE &&
        mce->signal_strength != -127) {
        obj_add = json_object_new_int(mce->signal_strength);

        if (!obj_add) {
            json_object_put(json_obj);
            return;
        } else {
            json_object_object_add(json_obj, ZMQ_KEY_SIGNAL, obj_add);
        }
    }

    if (mce->event_param != CONN_EVENT_META_UPDATE)
        return;

    retval = snprintf(topic, sizeof(topic), "%s.%s %s",
            MONROE_ZMQ_TOPIC_CONNECTIVITY,
            mce->interface_id,
            json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));

    if (retval < sizeof(topic))
        md_zeromq_send(mwz, topic, strlen(topic), 0);

    json_object_put(json_obj);
}


char imei_mapping [10][30];
int   maplen=0;
static int map_imei(const char* imei, struct md_writer_zeromq *mwz) {
    for (int i=0;i<maplen;i++) {
        if (strcmp(imei_mapping[i], imei)==0) {
            return i;
        }
    }
    FILE *fp = fopen("/tmp/interfaces", "r");
    if (fp==NULL) return -1;
    char* line = NULL;
    int nr=0;
    int match=-1;
    size_t len = 0;
    ssize_t read;
    while ((read = getline(&line, &len, fp)) != -1) {
        if(len>29) len=29;
        strncpy(imei_mapping[nr], line, len);
        imei_mapping[nr][strlen(imei_mapping[nr])-1]='\0';

        if (strcmp(imei_mapping[nr], imei)==0) match=nr;
        if (++nr==10) break;
    }
    maplen=nr;
    fclose(fp);
    if (line) free(line);
    return match;
}

static json_object *md_zeromq_create_iface_json(struct md_iface_event *mie, struct md_writer_zeromq *mwz)
{
    struct json_object *obj = NULL;

    if (!(obj = json_object_new_object()))
        return NULL;

    md_zeromq_add_default_fields(obj, mie->sequence, mie->tstamp, MONROE_ZMQ_DATA_ID_MODEM);

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, mie->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, mie->tstamp) ||
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, mie->iccid) ||
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, mie->imsi) ||
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, mie->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->isp_name && !md_zeromq_create_json_string(obj, ZMQ_KEY_ISP_NAME,
                mie->isp_name)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->ifname && mie->imei) {
        int iifindex=map_imei(mie->imei, mwz);
        if (iifindex > -1) {
            char iifname[4]="opX";
            iifname[2]=iifindex + '0';
            if (!md_zeromq_create_json_string(obj, ZMQ_KEY_IIF_NAME, iifname)) {
                json_object_put(obj);
                return NULL;
            }
        }
    }

    if (mie->ip_addr && !md_zeromq_create_json_string(obj, ZMQ_KEY_IP_ADDR,
                mie->ip_addr)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->internal_ip_addr && !md_zeromq_create_json_string(obj,
                ZMQ_KEY_INTERNAL_IP_ADDR, mie->internal_ip_addr)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->ifname && !md_zeromq_create_json_string(obj, ZMQ_KEY_IF_NAME,
                mie->ifname)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->imsi_mccmnc &&
            !md_zeromq_create_json_int64(obj, ZMQ_KEY_IMSI_MCCMNC, mie->imsi_mccmnc)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->nw_mccmnc &&
            !md_zeromq_create_json_int64(obj, ZMQ_KEY_NW_MCCMNC, mie->nw_mccmnc)) {
        json_object_put(obj);
        return NULL;
    }

    if ((mie->cid > -1 && mie->lac > -1) &&
            (!md_zeromq_create_json_int(obj, ZMQ_KEY_LAC, mie->lac) ||
             !md_zeromq_create_json_int(obj, ZMQ_KEY_CID, mie->cid))) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->rscp != (int16_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_RSCP, mie->rscp)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_rsrp != (int16_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_RSRP, mie->lte_rsrp)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_freq &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_FREQ, mie->lte_freq)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->rssi != (int8_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_RSSI, mie->rssi)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->ecio != (int8_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_ECIO, mie->ecio)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_rssi != (int8_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_RSSI, mie->lte_rssi)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_rsrq != (int8_t) META_IFACE_INVALID &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_RSRQ, mie->lte_rsrq)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->device_mode && !md_zeromq_create_json_int(obj, ZMQ_KEY_DEVICE_MODE,
                mie->device_mode)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->device_submode && !md_zeromq_create_json_int(obj, ZMQ_KEY_DEVICE_SUBMODE,
                mie->device_submode)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_band && !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_BAND,
                mie->lte_band)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->device_state && !md_zeromq_create_json_int(obj, ZMQ_KEY_DEVICE_STATE,
                mie->device_state)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->lte_pci != 0xFFFF &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_LTE_PCI, mie->lte_pci)) {
        json_object_put(obj);
        return NULL;
    }

    if (mie->enodeb_id >= 0 &&
            !md_zeromq_create_json_int(obj, ZMQ_KEY_ENODEB_ID, mie->enodeb_id)) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static void md_zeromq_handle_iface(struct md_writer_zeromq *mwz,
                                   struct md_iface_event *mie)
{
    struct json_object *json_obj =  md_zeromq_create_iface_json(mie, mwz);
    char topic[8192] = {0};
    int retval = 0;

    if (json_obj == NULL)
        return;

    //Switch on topic
    switch (mie->event_param) {
    case IFACE_EVENT_DEV_STATE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_STATE,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_MODE_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_MODE,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_SIGNAL_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_SIGNAL,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_LTE_BAND_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_LTE_BAND,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_ISP_NAME_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_ISP_NAME,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_UPDATE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_UPDATE,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_IP_ADDR_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_IP_ADDR,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_LOC_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_LOC_CHANGE,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;
    case IFACE_EVENT_NW_MCCMNC_CHANGE:
        retval = snprintf(topic, sizeof(topic), "%s.%s.%s %s",
                MONROE_ZMQ_TOPIC_MODEM,
                mie->iccid,
                MONROE_ZMQ_TOPIC_MODEM_NW_MCCMNC_CHANGE,
                json_object_to_json_string_ext(json_obj, JSON_C_TO_STRING_PLAIN));
        break;

    default:
        json_object_put(json_obj);
        return;
    }

    if (retval >= sizeof(topic)) {
        json_object_put(json_obj);
        return;
    }

    md_zeromq_send(mwz, topic, strlen(topic), 0);
    json_object_put(json_obj);
}

static json_object *md_zeromq_handle_radio_cell_loc_gerant(
        struct md_writer_zeromq *mwz,
        struct md_radio_cell_loc_geran_event *event)
{
    struct json_object *obj, *geran_info;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, event->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, event->tstamp) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_CELL_ID, event->cell_id) ||
        !md_zeromq_create_json_string(obj, ZMQ_KEY_RADIO_PLMN, event->plmn) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_LAC, event->lac) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_ARFCN, event->arfcn) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_BSIC, event->bsic) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_TIMING_ADVANCE, event->timing_advance) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_RX_LEV, event->rx_lev)){
        json_object_put(obj);
        return NULL;
    }

    if (event->cell_geran_info_nmr) {
        geran_info = json_tokener_parse(event->cell_geran_info_nmr);

        if (geran_info) {
            json_object_object_add(obj, ZMQ_KEY_RADIO_CELL_GERAN_INFO_NMR, geran_info);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse geran obj\n");
            json_object_put(obj);
            return NULL;
        }
    }

    return obj;
}

static json_object *md_zeromq_handle_radio_cell_resel_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_grr_cell_resel_event *event)
{
    struct json_object *obj, *neighbors;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, event->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, event->tstamp) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_SERVING_BCCH_ARFCN, event->serving_bcch_arfcn) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_SERVING_PBCCH_ARFCN, event->serving_pbcch_arfcn) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_SERVING_C1, event->serving_c1) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_SERVING_C2, event->serving_c2) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_SERVING_C31, event->serving_c31) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_RADIO_SERVING_C32, event->serving_c32) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_SERVING_PRIORITY_CLASS, event->serving_priority_class) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_SERVING_RXLEV_AVG, event->serving_rxlev_avg) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_SERVING_FIVE_SECOND_TIMER, event->serving_five_second_timer) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CELL_RESELET_STATUS, event->cell_reselet_status) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_RECENT_CELL_SELECTION, event->recent_cell_selection)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->neighbors) {
        neighbors = json_tokener_parse(event->neighbors);

        if (neighbors) {
            json_object_object_add(obj, ZMQ_KEY_RADIO_GRR_CELL_NEIGHBORS, neighbors);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse neighbor obj\n");
            json_object_put(obj);
            return NULL;
        }
    }
    return obj;
}

static json_object *md_zeromq_handle_radio_cipher_mode_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_cipher_mode_event *event)
{
    struct json_object *obj;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, event->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, event->tstamp) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CIPHERING_STATE, event->ciphering_state) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CIPHERING_ALGORITHM, event->ciphering_algorithm)) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static json_object *md_zeromq_handle_cell_reset_param_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_cell_sel_reset_param_event *event)
{
    struct json_object *obj;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, event->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, event->tstamp) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CELL_RESELECT_HYSTERESIS, event->cell_reselect_hysteresis) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_MS_TXPWR_MAX_CCH, event->ms_txpwr_max_cch) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_RXLEV_ACCESS_MIN, event->rxlev_access_min) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_POWER_OFFSET_VALID, event->power_offset_valid) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_POWER_OFFSET, event->power_offset) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_NECI, event->neci) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_ACS, event->acs) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_OPT_RESELECT_PARAM_IND, event->opt_reselect_param_ind) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CELL_BAR_QUALIFY, event->cell_bar_qualify) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CELL_RESELECT_OFFSET, event->cell_reselect_offset) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_TEMPORARY_OFFSET, event->temporary_offset) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_PENALTY_TIME, event->penalty_time)) {
        json_object_put(obj);
        return NULL;
    }

    return obj;
}

static json_object *md_zeromq_handle_rr_channel_conf_event(
        struct md_writer_zeromq *mwz,
        struct md_radio_gsm_rr_channel_conf_event *event)
{
    struct json_object *obj, *obj_tmp;

    if (!(obj = json_object_new_object()))
        return NULL;

    if (event->iccid &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_ICCID, event->iccid)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imsi &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMSI, event->imsi)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->imei &&
        !md_zeromq_create_json_string(obj, ZMQ_KEY_IMEI, event->imei)) {
        json_object_put(obj);
        return NULL;
    }

    if (!md_zeromq_create_json_int(obj, ZMQ_KEY_SEQ, event->sequence) ||
        !md_zeromq_create_json_int64(obj, ZMQ_KEY_TSTAMP, event->tstamp) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_NUM_DED_CHANS, event->num_ded_chans) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_DTX_INDICATOR, event->dtx_indicator) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_POWER_LEVEL, event->power_level) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_STARTING_TIME_VALID, event->starting_time_valid) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_STARTING_TIME, event->starting_time) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CIPHER_FLAG, event->cipher_flag) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CIPHER_ALGORITHM, event->cipher_algorithm) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CHANNEL_MODE_1, event->channel_mode_1) ||
        !md_zeromq_create_json_int(obj, ZMQ_KEY_RADIO_CHANNEL_MODE_2, event->channel_mode_2)) {
        json_object_put(obj);
        return NULL;
    }

    if (event->after_channel_config) {
        obj_tmp = json_tokener_parse(event->after_channel_config);

        if (obj_tmp) {
            json_object_object_add(obj, ZMQ_KEY_RADIO_AFTER_CHANNEL_CONFIG, obj_tmp);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse after conf\n");
            json_object_put(obj);
            return NULL;
        }
    }

    if (event->before_channel_config) {
        obj_tmp = json_tokener_parse(event->before_channel_config);

        if (obj_tmp) {
            json_object_object_add(obj, ZMQ_KEY_RADIO_BEFORE_CHANNEL_CONFIG, obj_tmp);
        } else {
            META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to parse before conf\n");
            json_object_put(obj);
            return NULL;
        }
    }

    return obj;
}

static void md_zeromq_handle_radio(struct md_writer_zeromq *mwz, 
                                   struct md_radio_event *mre)
{
    char msg[8192] = {0};
    struct json_object *obj = NULL;
    const char *topic = NULL;
    int32_t retval;

    switch (mre->event_param) {
    case RADIO_EVENT_GSM_RR_CIPHER_MODE:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CIPHER_MODE\n");
        topic = ZMQ_TOPIC_RADIO_GSM_RR_CIPHER_MODE;
        obj = md_zeromq_handle_radio_cipher_mode_event(mwz,
                (struct md_radio_gsm_rr_cipher_mode_event*) mre);
        break;
    case RADIO_EVENT_GSM_RR_CHANNEL_CONF:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CHANNEL_CONF\n");
        topic = ZMQ_TOPIC_RADIO_GSM_RR_CHANNEL_CONF;
        obj = md_zeromq_handle_rr_channel_conf_event(mwz,
                (struct md_radio_gsm_rr_channel_conf_event*) mre);
        break;
    case RADIO_EVENT_CELL_LOCATION_GERAN:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "ZMQ CELL_LOCATION_GERAN\n");
        topic = ZMQ_TOPIC_RADIO_CELL_LOCATION_GERAN;
        obj = md_zeromq_handle_radio_cell_loc_gerant(mwz,
                (struct md_radio_cell_loc_geran_event*) mre);
        break;
    case RADIO_EVENT_GSM_RR_CELL_SEL_RESEL_PARAM:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GSM_RR_CELL_SEL_RESEL_PARAM\n");
        topic = ZMQ_TOPIC_RADIO_GSM_RR_CELL_SEL_RESEL_PARAM;
        obj = md_zeromq_handle_cell_reset_param_event(mwz, 
                (struct md_radio_gsm_rr_cell_sel_reset_param_event*) mre);
        break;
    case RADIO_EVENT_GRR_CELL_RESEL:
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "GRR_CELL_RESEL\n");
        topic = ZMQ_TOPIC_RADIO_GRR_CELL_RESEL;
        obj = md_zeromq_handle_radio_cell_resel_event(mwz,
                (struct md_radio_grr_cell_resel_event*) mre);
        break;
    default:
        break;

    }

    if (!obj) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Failed to create JSON\n");
        return;
    }

    META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Will send %s %s\n", topic, msg);
    retval = snprintf(msg, sizeof(msg), "%s %s", topic,
            json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN));

    if (retval >= sizeof(msg))
        return;

    retval = zmq_send(mwz->zmq_publisher, msg, strlen(msg), 0);
    META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Sent %d %s\n", retval, msg);
}

static void md_zeromq_handle(struct md_writer *writer, struct md_event *event)
{
    struct md_writer_zeromq *mwz = (struct md_writer_zeromq*) writer;

    switch (event->md_type) {
    case META_TYPE_POS:
        md_zeromq_handle_gps(mwz, (struct md_gps_event*) event);
        break;
    case META_TYPE_CONNECTION:
        md_zeromq_handle_conn(mwz, (struct md_conn_event*) event);
        break;
    case META_TYPE_MUNIN:
        md_zeromq_handle_munin(mwz, (struct md_munin_event*) event);
        break;
    case META_TYPE_SYSEVENT:
        md_zeromq_handle_sysevent(mwz, (struct md_sysevent*) event);
        break;
    case META_TYPE_INTERFACE:
        md_zeromq_handle_iface(mwz, (struct md_iface_event*) event);
        break;
    case META_TYPE_RADIO:
        md_zeromq_handle_radio(mwz, (struct md_radio_event*) event);
        break;
    default:
        break;
    }
}

static uint8_t md_zeromq_config(struct md_writer_zeromq *mwz,
                                const char *address,
                                uint16_t port)
{
    //INET6_ADDRSTRLEN is 46 (max length of ipv6 + trailing 0), 5 is port, 6 is
    //protocol (we right now only support TCP)
    int32_t addrlen = INET6_ADDRSTRLEN + 5 + 6;

    mwz->zmq_addr = calloc(addrlen, 1);
    if (mwz->zmq_addr == NULL)
        return RETVAL_FAILURE;

    snprintf(mwz->zmq_addr, addrlen, "tcp://%s:%d", address, port);

    if ((mwz->zmq_context = zmq_ctx_new()) == NULL)
        return RETVAL_FAILURE;

    if ((mwz->zmq_publisher = zmq_socket(mwz->zmq_context, ZMQ_PUB)) == NULL)
        return RETVAL_FAILURE;

    if ((zmq_bind(mwz->zmq_publisher, mwz->zmq_addr)) != 0) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "zmq_bind failed (%d): %s - will try again.\n", errno,
                zmq_strerror(errno));
        mwz->connected = 0;
    } else {
        mwz->connected = 1;
    }

    META_PRINT_SYSLOG(mwz->parent, LOG_INFO, "ZeroMQ init done\n");

    return RETVAL_SUCCESS;
}

static int32_t md_zeromq_init(void *ptr, json_object* config)
{
    struct md_writer_zeromq *mwz = ptr;
    const char *address = NULL;
    uint16_t port = 0;

    json_object* subconfig;
    if (json_object_object_get_ex(config, "zmq", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "address"))
                address = json_object_get_string(val);
            if (!strcmp(key, "port"))
                port = (uint16_t) json_object_get_int(val);
        }
    }

    if (address == NULL || port == 0) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Missing required ZeroMQ argument\n");
        return RETVAL_FAILURE;
    }

    if (system_helpers_check_address(address)) {
        META_PRINT_SYSLOG(mwz->parent, LOG_ERR, "Error in ZeroMQ address\n");
        return RETVAL_FAILURE;
    }

    return md_zeromq_config(mwz, address, port);
}

void md_zeromq_usage()
{
    fprintf(stderr, "\"zmq\": {\t\tZeroMQ writer\n");
    fprintf(stderr, "  \"address\":\t\taddress used by publisher\n");
    fprintf(stderr, "  \"port\":\t\tport used by publisher\n");
    fprintf(stderr, "},\n");
}

void md_zeromq_setup(struct md_exporter *mde, struct md_writer_zeromq* mwz) {
    mwz->parent = mde;
    mwz->init = md_zeromq_init;
    mwz->handle = md_zeromq_handle;
}

