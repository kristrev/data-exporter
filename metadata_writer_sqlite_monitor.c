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
#include "metadata_writer_sqlite_monitor.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_json_helpers.h"
#include "metadata_exporter_log.h"

static uint8_t md_sqlite_monitor_dump_json(struct md_writer_sqlite *mws, FILE *output)
{
    const char *json_str;
    sqlite3_reset(mws->dump_monitor);
    json_object *jarray = json_object_new_array();

    if (md_json_helpers_dump_write(mws->dump_monitor, jarray)) {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    json_str = json_object_to_json_string_ext(jarray, JSON_C_TO_STRING_PLAIN);
    fprintf(output, "%s", json_str);

    json_object_put(jarray);
    return RETVAL_SUCCESS;
}

static uint8_t md_sqlite_monitor_delete_db(struct md_writer_sqlite *mws)
{
    int32_t retval = 0;
    sqlite3_reset(mws->delete_monitor);

    retval = sqlite3_step(mws->delete_monitor);

    if (retval == SQLITE_DONE) {
        return RETVAL_SUCCESS;
    } else {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to delete monitor table\n");
        return RETVAL_FAILURE;
    }
}

uint8_t md_sqlite_monitor_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = md_writer_helpers_copy_db(mws->monitor_prefix,
            mws->monitor_prefix_len, md_sqlite_monitor_dump_json, mws,
            md_sqlite_monitor_delete_db);

    if (retval == RETVAL_SUCCESS)
        mws->num_munin_events = 0;

    return retval;
}

uint8_t md_sqlite_handle_munin_event(struct md_writer_sqlite *mws,
                                   struct md_munin_event *mme)
{
    json_object *value;
    int64_t boottime    = 0;

    json_object *session_obj;
    if (!json_object_object_get_ex(mme->json_blob, "session", &session_obj)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to read data from session module (Munin)\n");
        return RETVAL_FAILURE;
    }
    if (!json_object_object_get_ex(session_obj, "start", &value)) 
        return RETVAL_FAILURE;
    if ((boottime = json_object_get_int64(value)) < 1400000000 ) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to read valid start time from session module (Munin): %" PRId64 "\n", boottime);
        return RETVAL_FAILURE;
    }

    sqlite3_stmt *stmt = mws->insert_monitor;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt,    1, mws->node_id)  ||
        sqlite3_bind_int(stmt,    2, mme->tstamp)   ||
        sqlite3_bind_int(stmt,    3, mme->sequence) || 
        sqlite3_bind_int64(stmt,  4, boottime)       ){ 
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query (Monitor)\n");
        return RETVAL_FAILURE;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE)
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}

