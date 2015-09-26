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
#include <unistd.h>
#include <getopt.h>
#include <libmnl/libmnl.h>
#include <json-c/json.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <sqlite3.h>

#include "metadata_writer_sqlite.h"
#include "metadata_writer_sqlite_conn.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_sqlite_gps.h"
#include "netlink_helpers.h"
#include "system_helpers.h"
#include "backend_event_loop.h"

static void md_sqlite_copy_db(struct md_writer_sqlite *mws, uint8_t from_timeout);
static void md_sqlite_handle_timeout(void *ptr);
static void md_sqlite_handle(struct md_writer *writer, struct md_event *event);

static void md_sqlite_itr_cb(void *ptr)
{
    struct md_writer_sqlite *mws = ptr;

    if (mws->file_failed && !mws->timeout_added) {
        mde_start_timer(mws->parent->event_loop,
                        mws->timeout_handle,
                        TIMEOUT_FILE);
        mws->timeout_added = 1; 
    }
}

static void md_sqlite_copy_db(struct md_writer_sqlite *mws, uint8_t from_timeout)
{
    uint8_t retval = RETVAL_FAILURE;
    uint8_t num_failed = 0;

    if (!mws->node_id)
        return;

    if (mws->timeout_added && !from_timeout) {
        backend_remove_timeout(mws->timeout_handle);
        mws->timeout_added = 0;
    }

    fprintf(stdout, "Will export DB. # meta %u # gps %u\n",
            mws->num_conn_events,
            mws->num_gps_events);

    if (mws->num_conn_events) {
        retval = md_sqlite_conn_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (mws->num_gps_events) {
        retval = md_sqlite_gps_copy_db(mws);

        if (retval == RETVAL_FAILURE)
            num_failed++;
    }

    if (num_failed != 0) {
        fprintf(stderr, "%u DB dump(s) failed\n", num_failed);
        mws->file_failed = 1;
    } else {
        mws->file_failed = 0;
    }
}

static uint8_t md_sqlite_update_nodeid_db(struct md_writer_sqlite *mws, const char *sql_str)
{
    int32_t retval;
    sqlite3_stmt *update_tables;

    if ((retval = sqlite3_prepare_v2(mws->db_handle, sql_str, -1,
                    &update_tables, NULL))) {
        fprintf(stderr, "Prepare failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }

    if ((retval = sqlite3_bind_int(update_tables, 1, mws->node_id))) {
        fprintf(stderr, "Bind failed %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE; 
    }
   
    retval = sqlite3_step(update_tables);

    if (retval != SQLITE_DONE) {
        fprintf(stderr, "Step faild %s\n", sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    sqlite3_finalize(update_tables);
    return RETVAL_SUCCESS;
}

static sqlite3* md_sqlite_configure_db(const char *db_filename)
{
    sqlite3 *db_handle = NULL;
    int retval = 0;
    char *db_errmsg = NULL;

    retval = sqlite3_open_v2(db_filename, &db_handle, SQLITE_OPEN_READWRITE |
                             SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);

    if (retval != SQLITE_OK) {
        if (db_handle != NULL)
            fprintf(stderr, "open failed with message: %s\n", sqlite3_errmsg(db_handle));
        else
            fprintf(stderr, "not enough memory to create db_handle object\n");

        return NULL;
    }

    //make sure database is ready to be used. this avoids having checks in
    //metadata_produce, since it will first export any message stored in
    //database
    if (sqlite3_exec(db_handle, CREATE_SQL, NULL, NULL, &db_errmsg)) {
        fprintf(stderr, "db create failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_UPDATE_SQL, NULL, NULL, &db_errmsg)) {
        fprintf(stderr, "db create (update) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    if (sqlite3_exec(db_handle, CREATE_GPS_SQL, NULL, NULL, &db_errmsg)) {
        fprintf(stderr, "db create (gps) failed with message: %s\n", db_errmsg);
        sqlite3_close_v2(db_handle);
        return NULL;
    }

    return db_handle;
}

static int md_sqlite_configure(struct md_writer_sqlite *mws,
        const char *db_filename, uint32_t node_id, uint32_t db_interval,
        uint32_t db_events, const char *meta_prefix, const char *gps_prefix)
{
    sqlite3 *db_handle = md_sqlite_configure_db(db_filename);
   
    if (db_handle == NULL)
        return RETVAL_FAILURE;

    //Only set variables that are not 0
    mws->db_handle = db_handle;
    mws->node_id = node_id;
    mws->db_interval = db_interval;
    mws->db_events = db_events;
    mws->do_fake_updates = 1;
    
    //We will not use timer right away
    if(!(mws->timeout_handle = backend_event_loop_create_timeout(0,
            md_sqlite_handle_timeout, mws, 0))) {
        sqlite3_close_v2(db_handle);
        return RETVAL_FAILURE;
    }
    
    if(sqlite3_prepare_v2(mws->db_handle, INSERT_PROVIDER, -1,
            &(mws->insert_provider), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_TABLE, -1,
            &(mws->delete_table), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_UPDATE, -1,
            &(mws->insert_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, UPDATE_UPDATE, -1,
            &(mws->update_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_EVENTS, -1,
            &(mws->dump_table), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_UPDATES, -1,
            &(mws->dump_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, SELECT_LAST_UPDATE, -1,
            &(mws->last_update), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, INSERT_GPS_EVENT, -1,
            &(mws->insert_gps), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DELETE_GPS_TABLE, -1,
            &(mws->delete_gps), NULL) ||
       sqlite3_prepare_v2(mws->db_handle, DUMP_GPS, -1,
            &(mws->dump_gps), NULL)){
        fprintf(stderr, "Statement failed: %s\n", sqlite3_errmsg(mws->db_handle));
        sqlite3_close_v2(db_handle);
        return RETVAL_FAILURE;
    }

    if (meta_prefix) {
        memset(mws->meta_prefix, 0, sizeof(mws->meta_prefix));
        memcpy(mws->meta_prefix, meta_prefix, strlen(meta_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->meta_prefix_len = strlen(meta_prefix);
    }

    if (gps_prefix) {
        memset(mws->gps_prefix, 0, sizeof(mws->gps_prefix));
        memcpy(mws->gps_prefix, gps_prefix, strlen(gps_prefix));

        //We need to reset the last six characthers to X, so keep track of the
        //length of the original prefix
        mws->gps_prefix_len = strlen(gps_prefix);
    }

    if (mws->node_id && (md_sqlite_update_nodeid_db(mws, UPDATE_EVENT_ID) ||
        md_sqlite_update_nodeid_db(mws, UPDATE_UPDATES_ID))) {
        fprintf(stderr, "Could not update old ements with id 0\n");
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

static void md_sqlite_usage()
{
    fprintf(stderr, "SQLite writer. At least one prefix is required:\n");
    fprintf(stderr, "--sql_database: path to database (local files only) (r)\n");
    fprintf(stderr, "--sql_nodeid: node id. If not specified, will be read from system (compile option)\n");
    fprintf(stderr, "--sql_meta_prefix: location + filename prefix for connection metadata ( max 116 characters)\n");
    fprintf(stderr, "--sql_gps_prefix: location + filename prefix for GPS data (max 116 characters)\n");
    fprintf(stderr, "--sql_interval: time (in ms) from event and until database is copied (default: 5 sec)\n");
    fprintf(stderr, "--sql_events: number of events before copying database\n (default: 10)\n");
}

int32_t md_sqlite_init(void *ptr, int argc, char *argv[])
{
    struct md_writer_sqlite *mws = ptr;
    uint32_t node_id = 0, interval = DEFAULT_TIMEOUT, num_events = EVENT_LIMIT;
    int c, option_index = 0;
    const char *db_filename = NULL, *meta_prefix = NULL, *gps_prefix = NULL;

    static struct option sqlite_options[] = {
        {"sql_database",         required_argument,  0,  0},
        {"sql_nodeid",           required_argument,  0,  0},
        {"sql_meta_prefix",      required_argument,  0,  0},
        {"sql_gps_prefix",       required_argument,  0,  0},
        {"sql_interval",         required_argument,  0,  0},
        {"sql_events",           required_argument,  0,  0},
        {0,                                      0,  0,  0}};

    while (1) {
        //No permuting of array here as well
        c = getopt_long_only(argc, argv, "--", sqlite_options, &option_index);

        if (c == -1)
            break;
        else if (c)
            continue;

        if (!strcmp(sqlite_options[option_index].name, "sql_database"))
            db_filename = optarg;
        else if (!strcmp(sqlite_options[option_index].name, "sql_nodeid"))
            node_id = (uint32_t) atoi(optarg);
        else if (!strcmp(sqlite_options[option_index].name, "sql_meta_prefix"))
            meta_prefix = optarg;
        else if (!strcmp(sqlite_options[option_index].name, "sql_gps_prefix"))
            gps_prefix = optarg;
        else if (!strcmp(sqlite_options[option_index].name, "sql_interval"))
            interval = ((uint32_t) atoi(optarg)) * 1000;
        else if (!strcmp(sqlite_options[option_index].name, "sql_events"))
            num_events = (uint32_t) atoi(optarg);
    }

    if (!db_filename || (!gps_prefix && !meta_prefix)) {
        fprintf(stderr, "Required SQLite argument missing\n");
        return RETVAL_FAILURE;
    }

    if ((meta_prefix && strlen(meta_prefix) > 117) ||
        (gps_prefix && strlen(gps_prefix) > 117)) {
        fprintf(stderr, "SQLite temp file prefix too long\n");
        return RETVAL_FAILURE;
    }

    if (!interval || !num_events) {
        fprintf(stderr, "Invalid SQLite interval/number of events\n");
        return RETVAL_FAILURE;
    }
  
    fprintf(stdout, "Done configuring SQLite handle\n");

    return md_sqlite_configure(mws, db_filename, node_id, interval,
            num_events, meta_prefix, gps_prefix);
}

static void md_sqlite_handle(struct md_writer *writer, struct md_event *event)
{
    uint8_t retval = RETVAL_SUCCESS;
    struct md_writer_sqlite *mws = (struct md_writer_sqlite*) writer;

    switch (event->md_type) {
    case META_TYPE_CONNECTION:
        if (!mws->meta_prefix[0])
            return;

        retval = md_sqlite_handle_conn_event(mws, (struct md_conn_event*) event);
        if (!retval)
            mws->num_conn_events++;
        break;
    case META_TYPE_POS:
        if (!mws->gps_prefix[0])
            return;

        retval = md_sqlite_handle_gps_event(mws, (struct md_gps_event*) event);
        if (!retval)
            mws->num_gps_events++;
        break;
    default:
        fprintf(stdout, "SQLite writer does not support event %u\n",
                event->md_type);
        return;
    }

    if (retval == RETVAL_FAILURE) {
        fprintf(stderr, "Failed/ignored to insert JSON in DB\n");
        return;
    }

    //Something failed when dumping database, we have already rearmed timer for
    //checking again
    if (mws->file_failed)
        return;

    //These two are exclusive. There is no point adding timeout if event_limit
    //is hit. This can happen if event_limit is 1. The reason we do not use
    //lte is that if a copy fails, we deal with that in a timeout
    if ((mws->num_conn_events + mws->num_gps_events) == mws->db_events) {
        md_sqlite_copy_db(mws, 0);
    } else if (!mws->timeout_added) {
        mde_start_timer(mws->parent->event_loop, mws->timeout_handle,
                        mws->db_interval);
        mws->timeout_added = 1;
    }
}

static void md_sqlite_handle_timeout(void *ptr)
{
    struct md_writer_sqlite *mws = ptr;

    if (mws->file_failed)
        fprintf(stdout, "DB export retry\n");
    else
        fprintf(stdout, "Will export DB after timeout\n");

    if(!mws->node_id) {
        mws->node_id = system_helpers_get_nodeid();

        if(mws->node_id)
            fprintf(stdout, "Got nodeid %d\n", mws->node_id);
       
        if (!mws->node_id) {
            fprintf(stdout, "No node id found\n");
            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            return;
        }

        if (md_sqlite_update_nodeid_db(mws, UPDATE_EVENT_ID) ||
            md_sqlite_update_nodeid_db(mws, UPDATE_UPDATES_ID)) {
            fprintf(stderr, "Could not update node id in database\n");

            mws->timeout_handle->intvl = DEFAULT_TIMEOUT;
            //TODO: Work-around for making sure we check the node id on next
            //timeout, we should do something nicer
            mws->node_id = 0;
            return;
        }
    }

    //TODO: Add support for the online backup API provided by SQLite
    //TODO: READDDDDD
    md_sqlite_copy_db(mws, 1);

    //If we get here, then timeout has been processed. If copy_db has failed,
    //timeout will be started by the iteration callback. Intvl is set to 0 here
    //to make sure that we reset interval
    mws->timeout_added = 0;
    mws->timeout_handle->intvl = 0;
}

void md_sqlite_setup(struct md_exporter *mde, struct md_writer_sqlite* mws) {
    mws->parent = mde;
    mws->init = md_sqlite_init;
    mws->handle = md_sqlite_handle;
    mws->itr_cb = md_sqlite_itr_cb;
    mws->usage = md_sqlite_usage;
}

