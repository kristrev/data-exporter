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

#include <stdint.h>
#include <string.h>
#include <sqlite3.h>
#include <sys/time.h>

#include "metadata_exporter.h"
#include "metadata_writer_inventory_gps.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_json_helpers.h"
#include "metadata_exporter_log.h"

static uint8_t md_inventory_gps_dump_db_sql(struct md_writer_sqlite *mws, FILE *output)
{
    sqlite3_reset(mws->dump_gps);

    if (md_sqlite_helpers_dump_write(mws->dump_gps, output))
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}

static uint8_t md_inventory_gps_dump_db_json(struct md_writer_sqlite *mws, FILE *output)
{
    const char *json_str;
    sqlite3_reset(mws->dump_gps);

    json_object *jarray = json_object_new_array();

    if (md_json_helpers_dump_write(mws->dump_gps, jarray) ||
        json_object_array_length(jarray) == 0)
    {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    json_str = json_object_to_json_string_ext(jarray, JSON_C_TO_STRING_PLAIN);
    fprintf(output, "%s", json_str);

    json_object_put(jarray);
    return RETVAL_SUCCESS;
}

uint8_t md_inventory_gps_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = RETVAL_SUCCESS;
    dump_db_cb dump_cb = NULL;

    if (!strcmp(mws->parent->output_format, "sql")) {
        dump_cb = md_inventory_gps_dump_db_sql;
    } else {
        dump_cb = md_inventory_gps_dump_db_json;
    }

    retval = md_writer_helpers_copy_db(mws->gps_prefix,
            mws->gps_prefix_len, dump_cb, mws,
            NULL);

    if (retval == RETVAL_SUCCESS)
        mws->num_gps_events = 0;

    return retval;
}

uint8_t md_inventory_handle_gps_event(struct md_writer_sqlite *mws,
                                   struct md_gps_event *mge)
{
    if (mge->speed)
        mws->gps_speed = mge->speed;

    if (mge->minmea_id == MINMEA_SENTENCE_RMC)
        return RETVAL_IGNORE;

    //We dont need EVERY gps event, some devices send updates very frequently
    //Some of the devices we work with have timers that are ... strange
    if (mws->last_gps_insert > mge->tstamp_tv.tv_sec ||
        mge->tstamp_tv.tv_sec - mws->last_gps_insert < GPS_EVENT_INTVL)
        return RETVAL_IGNORE;

    sqlite3_stmt *stmt = mws->insert_gps;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int(stmt, 2, mws->session_id) ||          // BootCount
        sqlite3_bind_int(stmt, 3, mws->session_id_multip) ||   // BootMultiplier
        sqlite3_bind_int(stmt, 4, mge->tstamp_tv.tv_sec) ||
        sqlite3_bind_int(stmt, 5, mge->sequence) ||
        sqlite3_bind_double(stmt, 6, mge->latitude) ||
        sqlite3_bind_double(stmt, 7, mge->longitude)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query (GPS)\n");
        return RETVAL_FAILURE;
    }

    if (mge->altitude &&
        sqlite3_bind_double(stmt, 8, mge->altitude)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind altitude\n");
        return RETVAL_FAILURE;
    }

    if (mws->gps_speed &&
        sqlite3_bind_double(stmt, 9, mws->gps_speed)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind speed\n");
        return RETVAL_FAILURE;
    }

    if (mge->satellites_tracked &&
        sqlite3_bind_int(stmt, 10, mge->satellites_tracked)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind num. satelites\n");
        return RETVAL_FAILURE;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        return RETVAL_FAILURE;
    } else {
        mws->last_gps_insert = mge->tstamp_tv.tv_sec;
        return RETVAL_SUCCESS;
    }
}

