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
#include <sqlite3.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "metadata_utils.h"
#include "metadata_exporter.h"
#include "metadata_writer_sqlite_conn.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_exporter_log.h"

static int32_t md_sqlite_execute_insert_update(struct md_writer_sqlite *mws,
                                               struct md_conn_event *mce)
{
    sqlite3_stmt *stmt = mws->insert_update;
    
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int64(stmt, 2, mws->session_id) ||
        sqlite3_bind_int64(stmt, 3, mws->session_id_multip) ||
        sqlite3_bind_int64(stmt, 4, mce->tstamp) ||
        sqlite3_bind_int(stmt, 5, mce->sequence) ||
        sqlite3_bind_int(stmt, 6, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 7, mce->l4_session_id) ||
        sqlite3_bind_text(stmt, 8, mce->event_value_str, strlen(mce->event_value_str), SQLITE_STATIC) ||
        sqlite3_bind_int(stmt, 9, mce->interface_type) ||
        sqlite3_bind_text(stmt, 11, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)){
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 10, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 10, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }


    if (mce->network_provider &&
        sqlite3_bind_int(stmt, 12, mce->network_provider)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind network provider\n");
        return SQLITE_ERROR;
    }

    return sqlite3_step(stmt);
}

static int32_t md_sqlite_execute_insert(struct md_writer_sqlite *mws,
                                        struct md_conn_event *mce)
{
    int32_t retval;
   
    sqlite3_stmt *stmt = mws->insert_event;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int64(stmt, 2, mws->session_id) ||
        sqlite3_bind_int64(stmt, 3, mws->session_id_multip) ||
        sqlite3_bind_int64(stmt, 4, mce->tstamp) ||
        sqlite3_bind_int(stmt, 5, mce->sequence) ||
        sqlite3_bind_int(stmt, 6, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 7, mce->l4_session_id) ||
        sqlite3_bind_int(stmt, 8, mce->event_type) ||
        sqlite3_bind_int(stmt, 9, mce->event_param) ||
        sqlite3_bind_int(stmt, 12, mce->interface_type) ||
        sqlite3_bind_int(stmt, 13, mce->interface_id_type) ||
        sqlite3_bind_int(stmt, 16, mce->network_address_family) ||
        sqlite3_bind_text(stmt, 17, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 14, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 14, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    if (mce->event_value != UINT8_MAX &&
        sqlite3_bind_int(stmt, 10, mce->event_value)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed bind event value (int)\n");
        return SQLITE_ERROR;
    }

    if (mce->event_value_str != NULL &&
        sqlite3_bind_text(stmt, 11, mce->event_value_str, strlen(mce->event_value_str), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind event value (string)\n");
        return SQLITE_ERROR;
    }

    if (mce->network_provider) {
        retval = sqlite3_bind_int(stmt, 15, mce->network_provider);

        if (retval) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind provider to INSERT query\n");
            return SQLITE_ERROR;
        }
    }

    return sqlite3_step(stmt);
}

static int32_t md_sqlite_update_event(struct md_writer_sqlite *mws,
                                      struct md_conn_event *mce)
{
    sqlite3_stmt *stmt = mws->update_update;

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int64(stmt, 1, mce->tstamp) ||
        sqlite3_bind_text(stmt, 2, mce->event_value_str, strlen(mce->event_value_str), SQLITE_STATIC) ||
        sqlite3_bind_int(stmt, 3, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 4, mce->l4_session_id) ||
        sqlite3_bind_text(stmt, 5, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE query\n");
        return SQLITE_ERROR;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 6, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 6, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    return sqlite3_step(stmt);
}

static int32_t md_sqlite_execute_insert_usage(struct md_writer_sqlite *mws,
                                              struct md_conn_event *mce,
                                              uint64_t date_start)
{
    uint8_t interface_id_idx = 1;
    sqlite3_stmt *stmt = mws->insert_usage;
    const char *no_iccid_str = "0";

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    //For modems we need both IMEI and ICCID. ICCID is currently stored in the
    //interface_id variable, so some special handling is needed for now
    if (mce->imei) {
        if (sqlite3_bind_text(stmt, 1, mce->imei, strlen(mce->imei), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 3, mce->imsi, strlen(mce->imsi), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI/IMSI\n");
            return SQLITE_ERROR;
        }

        interface_id_idx = 2;
    } else {
        if (sqlite3_bind_text(stmt, 2, no_iccid_str, strlen(no_iccid_str), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 3, no_iccid_str, strlen(no_iccid_str), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind empty IMEI/IMSI\n");
            return SQLITE_ERROR;
        }
    }

    if (sqlite3_bind_text(stmt, interface_id_idx, mce->interface_id,
            strlen(mce->interface_id), SQLITE_STATIC) ||
        sqlite3_bind_int64(stmt, 4, date_start) ||
        sqlite3_bind_int64(stmt, 5, mce->rx_bytes) ||
        sqlite3_bind_int64(stmt, 6, mce->tx_bytes)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT usage query\n");
        return SQLITE_ERROR;
    }

    return sqlite3_step(stmt);
}

static int32_t md_sqlite_execute_update_usage(struct md_writer_sqlite *mws,
                                              struct md_conn_event *mce,
                                              uint64_t date_start)
{
    const char *no_iccid_str = "0";
    sqlite3_stmt *stmt = mws->update_usage;

    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int64(stmt, 1, mce->rx_bytes) ||
        sqlite3_bind_int64(stmt, 2, mce->tx_bytes) ||
        sqlite3_bind_int64(stmt, 6, date_start)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query\n");
        return SQLITE_ERROR;
    }
        
    if (mce->imei) {
        if (sqlite3_bind_text(stmt, 3, mce->imei, strlen(mce->imei),
                SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 4, mce->interface_id,
                strlen(mce->interface_id), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 5, mce->imsi,
                strlen(mce->imsi), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query #2\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 3, mce->interface_id,
                strlen(mce->interface_id), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 4, no_iccid_str,
                strlen(no_iccid_str), SQLITE_STATIC) ||
            sqlite3_bind_text(stmt, 5, no_iccid_str,
                strlen(no_iccid_str), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to UPDATE usage query #2\n");
            return SQLITE_ERROR;
        }
    }

    return sqlite3_step(stmt);
}

static uint8_t md_sqlite_handle_insert_conn_event(struct md_writer_sqlite *mws,
                                                  struct md_conn_event *mce)
{
    int32_t retval = md_sqlite_execute_insert(mws, mce);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "INSERT failed: %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    mws->num_conn_events++;
    return RETVAL_SUCCESS;
}

static int16_t md_sqlite_get_last_update(struct md_writer_sqlite *mws,
                                         struct md_conn_event *mce,
                                         int16_t *mode, int16_t *quality)
{
    int16_t retval = -1;
    int numbytes = 0;
    const unsigned char *event_value_str = NULL;
    char event_str_cpy[EVENT_STR_LEN];
    sqlite3_stmt *stmt = mws->last_update;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    *mode = -1;
    *quality = -1;

    if (sqlite3_bind_int(stmt, 1, mce->l3_session_id) ||
        sqlite3_bind_int(stmt, 2, mce->l4_session_id) ||
        sqlite3_bind_text(stmt, 4, mce->network_address, strlen(mce->network_address), SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to SELECT query\n");
        return retval;
    }

    if (mws->api_version == 2 && mce->interface_type == INTERFACE_MODEM) {
        if (sqlite3_bind_text(stmt, 3, mce->imei, strlen(mce->imei), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind IMEI\n");
            return SQLITE_ERROR;
        }
    } else {
        if (sqlite3_bind_text(stmt, 3, mce->interface_id, strlen(mce->interface_id), SQLITE_STATIC)) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind interface id\n");
            return SQLITE_ERROR;
        }
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        event_value_str = sqlite3_column_text(stmt, 0);
        numbytes = sqlite3_column_bytes(stmt, 0);

        if (numbytes >= EVENT_STR_LEN) {
            META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Event value string will not fit in buffer\n");
            return SQLITE_ERROR;
        }

        memcpy(event_str_cpy, event_value_str, numbytes);
        event_str_cpy[numbytes] = '\0';

        *mode = metadata_utils_get_csv_pos(event_str_cpy, 2);
        *quality = metadata_utils_get_csv_pos(event_str_cpy, 3);
        break;
    }

    return retval;
}

static void md_sqlite_insert_fake_mode(struct md_writer_sqlite *mws,
                                       struct md_conn_event *mce,
                                       uint8_t mode)
{
    const char *event_value_str = mce->event_value_str;
    int32_t retval;

    //TODO: Not a nice way to access parent variable
    mce->sequence = mde_inc_seq(mws->parent);
    mce->event_value = mode;
    mce->event_param = CONN_EVENT_META_MODE_UPDATE;
    mce->event_value_str = NULL;
    
    retval = md_sqlite_execute_insert(mws, mce);

    if (retval == SQLITE_DONE)
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Inserted fake mode update\n");
    else
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to insert fake mode update\n");

    //Restore/update query after mode insert
    mce->event_param = CONN_EVENT_META_UPDATE;
    mce->event_value = 0;
    mce->event_value_str = event_value_str;
}

static void md_sqlite_insert_fake_quality(struct md_writer_sqlite *mws,
                                          struct md_conn_event *mce,
                                          uint8_t quality)
{
    const char *event_value_str = mce->event_value_str;
    int32_t retval;

    mce->sequence = mde_inc_seq(mws->parent);
    mce->event_value = quality;
    mce->event_param = CONN_EVENT_META_QUALITY_UPDATE;
    mce->event_value_str = NULL;
    
    retval = md_sqlite_execute_insert(mws, mce);

    if (retval == SQLITE_DONE)
        META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Inserted fake quality update\n");
    else
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to insert fake quality update\n");

    //Restore/update query after quality insert
    mce->event_param = CONN_EVENT_META_UPDATE;
    mce->event_value = 0;
    mce->event_value_str = event_value_str;
}

static void md_sqlite_insert_fake_events(struct md_writer_sqlite *mws,
                                         struct md_conn_event *mce,
                                         int32_t update_exists)
{
    //TODO: Find a way to respect const
    int16_t mode_in_update = -1, mode_in_table = -1;
    int16_t quality_in_update = -1, quality_in_table = -1;
    char event_str_cpy[EVENT_STR_LEN];
    size_t event_str_len;
    struct timeval t_now;

    if (mws->first_fake_update.tv_sec != 0) {
        gettimeofday(&t_now, NULL);

        if (t_now.tv_sec > mws->first_fake_update.tv_sec &&
            t_now.tv_sec - mws->first_fake_update.tv_sec > FAKE_UPDATE_LIMIT) {
            mws->do_fake_updates = 0;
            return;
        }
    } else {
        gettimeofday(&(mws->first_fake_update), NULL); 
    }

    event_str_len = strlen(mce->event_value_str);

    if (event_str_len >= EVENT_STR_LEN) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Event string too long\n");
        return;
    }

    memcpy(event_str_cpy, mce->event_value_str, event_str_len);
    event_str_cpy[event_str_len] = '\0';

    mode_in_update = metadata_utils_get_csv_pos(event_str_cpy, 2);
    quality_in_update = metadata_utils_get_csv_pos(event_str_cpy, 3);

    //If there was no update messge from before, then insert the fake mode/quality messages (just to be sure)
    //and return
    if (update_exists == SQLITE_DONE) {
        //Always insert mode on the first update, for consistency (it should be
        //possible to follow the mode update messages exclusively)
        if (mode_in_update != -1)
            md_sqlite_insert_fake_mode(mws, mce, mode_in_update);

        if (quality_in_update != -1)
            md_sqlite_insert_fake_quality(mws, mce, quality_in_update);

        return;
    }

    md_sqlite_get_last_update(mws, mce, &mode_in_table, &quality_in_table);

    //Get mode from last update message. If we can read modem mode, then this
    //value will be 0 or larger
    if (mode_in_update != -1 && mode_in_update != mode_in_table)
        md_sqlite_insert_fake_mode(mws, mce, mode_in_update);

    if (quality_in_update != -1 && quality_in_update != quality_in_table)
        md_sqlite_insert_fake_quality(mws, mce, quality_in_update);
}

static uint8_t md_sqlite_handle_update_event(struct md_writer_sqlite *mws,
                                             struct md_conn_event *mce)
{
    //Check if update is present in update table by doing an insert
    int32_t retval = md_sqlite_execute_insert_update(mws, mce);

    if (mws->do_fake_updates)
        md_sqlite_insert_fake_events(mws, mce, retval);

    //No need to do UPDATE if INSERT was successful
    if (retval == SQLITE_DONE)
        return RETVAL_SUCCESS;

    //Update in update table
    retval = md_sqlite_update_event(mws, mce);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "UPDATE failed: %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    mws->num_conn_events++;
    return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_handle_usage_update(struct md_writer_sqlite *mws,
                                             struct md_conn_event *mce)
{
    uint64_t date_start = 0;
    struct tm tm_tmp = {0};
    time_t tstamp = (time_t) mce->tstamp;
    int32_t retval;

    //Create correct date_start and date_end (always to the hour) and keep at 0
    //if not, so that we can more easily update
    gmtime_r(&tstamp, &tm_tmp);

    //Only keep hour for date_start, date_end
    tm_tmp.tm_sec = 0;
    tm_tmp.tm_min = 0;

    date_start = (uint64_t) timegm(&tm_tmp);

    retval = md_sqlite_execute_update_usage(mws, mce, date_start);

    if (retval == SQLITE_DONE && sqlite3_changes(mws->db_handle)) {
        mws->num_usage_events++;
        return RETVAL_SUCCESS;
    }

    retval = md_sqlite_execute_insert_usage(mws, mce, date_start);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to update usage\n");
        return RETVAL_FAILURE;
    }

    mws->num_usage_events++;
    return RETVAL_SUCCESS;
}

uint8_t md_sqlite_handle_conn_event(struct md_writer_sqlite *mws,
                                           struct md_conn_event *mce)
{
    uint8_t retval = RETVAL_SUCCESS;

    //Not quite sure how to handle timestamps that would go back in time
    if (mce->tstamp > mws->last_msg_tstamp)
        mws->last_msg_tstamp = mce->tstamp;

    if (mce->event_param == CONN_EVENT_META_UPDATE) {
        retval = md_sqlite_handle_update_event(mws, mce);
    } else if (mce->event_param == CONN_EVENT_DATA_USAGE_UPDATE) {
        if (mws->usage_prefix[0] && (mce->rx_bytes || mce->tx_bytes))
            retval = md_sqlite_handle_usage_update(mws, mce);
    } else {
        retval = md_sqlite_handle_insert_conn_event(mws, mce);
    }

    return retval;
}

static uint8_t md_sqlite_conn_dump_db(struct md_writer_sqlite *mws, FILE *output)
{
    sqlite3_reset(mws->dump_table);
    sqlite3_reset(mws->dump_update);

    sqlite3_bind_int64(mws->dump_table, 1, mws->dump_tstamp);
    sqlite3_bind_int64(mws->dump_update, 1, mws->dump_tstamp);
    
    if (md_sqlite_helpers_dump_write(mws->dump_table, output) ||
        md_sqlite_helpers_dump_write(mws->dump_update, output))
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_usage_dump_db(struct md_writer_sqlite *mws, FILE *output)
{
    sqlite3_reset(mws->dump_usage);
    
    if (md_sqlite_helpers_dump_write(mws->dump_usage, output))
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}

uint8_t md_sqlite_conn_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = md_writer_helpers_copy_db(mws->meta_prefix,
            mws->meta_prefix_len, md_sqlite_conn_dump_db, mws,
            mws->delete_table);
   
    if (retval == RETVAL_SUCCESS) {
        mws->dump_tstamp = mws->last_msg_tstamp;
        mws->num_conn_events = 0;

        if (mws->last_conn_tstamp_path)
            system_helpers_write_uint64_to_file(mws->last_conn_tstamp_path,
                    mws->dump_tstamp);
    }

    return retval;

}

uint8_t md_sqlite_conn_usage_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = md_writer_helpers_copy_db(mws->usage_prefix,
            mws->usage_prefix_len, md_sqlite_usage_dump_db, mws,
            mws->delete_usage);
   
    if (retval == RETVAL_SUCCESS)
        mws->num_usage_events = 0;

    return retval;
}
