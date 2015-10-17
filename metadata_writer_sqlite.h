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

#ifndef METADATA_WRITER_SQLITE_H
#define METADATA_WRITER_SQLITE_H

#include <sys/time.h>
#include <sqlite3.h>
#include "metadata_exporter.h"

#define DEFAULT_TIMEOUT 5000
#define TIMEOUT_FILE 1000
#define EVENT_LIMIT 10
#define MAX_PATH_LEN 128
#define FAKE_UPDATE_LIMIT 120

#define CREATE_SQL          "CREATE TABLE IF NOT EXISTS NetworkEvent(" \
                            "NodeId INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER," \
                            "EventType INTEGER NOT NULL," \
                            "EventParam INTEGER NOT NULL," \
                            "EventValue INTEGER," \
                            "EventValueStr TEXT," \
                            "InterfaceType INTEGER NOT NULL," \
                            "InterfaceIdType INTEGER NOT NULL," \
                            "InterfaceId TEXT NOT NULL," \
                            "NetworkProvider INT," \
                            "NetworkAddressFamily INTEGER NOT NULL," \
                            "NetworkAddress TEXT NOT NULL," \
                            "PRIMARY KEY(NodeId,Timestamp,Sequence))"

#define CREATE_UPDATE_SQL   "CREATE TABLE IF NOT EXISTS NetworkUpdates(" \
                            "NodeId INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER NOT NULL DEFAULT 0," \
                            "EventValueStr TEXT NOT NULL," \
                            "InterfaceType INTEGERNOT NULL," \
                            "InterfaceId TEXT NOT NULL," \
                            "NetworkAddress TEXT NOT NULL," \
                            "NetworkProvider INT," \
                            "PRIMARY KEY(L3SessionId,L4SessionId,InterfaceId,NetworkAddress))"

#define CREATE_GPS_SQL      "CREATE TABLE IF NOT EXISTS GpsEvents(" \
                            "NodeId INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "Latitude REAL NOT NULL," \
                            "Longitude REAL NOT NULL," \
                            "Altitude REAL," \
                            "GroundSpeed REAL," \
                            "NumOfSatelites INTEGER," \
                            "PRIMARY KEY(NodeId,Timestamp,Sequence))"

#define INSERT_PROVIDER     "INSERT INTO NetworkEvent(NodeId,Timestamp" \
                            ",Sequence,L3SessionId,L4SessionId,EventType" \
                            ",EventParam,EventValue,EventValueStr,InterfaceType" \
                            ",InterfaceIdType,InterfaceId" \
                            ",NetworkProvider" \
                            ",NetworkAddressFamily,NetworkAddress) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_UPDATE       "INSERT INTO NetworkUpdates(NodeId,Timestamp" \
                            ",Sequence,L3SessionId,L4SessionId" \
                            ",EventValueStr,InterfaceType,InterfaceId" \
                            ",NetworkAddress,NetworkProvider) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?)"

#define INSERT_GPS_EVENT    "INSERT INTO GpsEvents(NodeId,Timestamp" \
                            ",Sequence,Latitude,Longitude,Altitude" \
                            ",GroundSpeed,NumOfSatelites) " \
                            "VALUES (?,?,?,?,?,?,?,?)"

#define SELECT_LAST_UPDATE  "SELECT EventValueStr FROM NetworkUpdates WHERE "\
                            "L3SessionId=? AND "\
                            "L4SessionId=? AND InterfaceId=? AND "\
                            "NetworkAddress=? ORDER BY Timestamp DESC LIMIT 1;"

//The reason NetworkAddress is used here, is that we first identify by L3/L4
//session. However, it might be that multiple addresses on same interface has
//the same L3/L4 ID (since we use timestamp). (L3 ID,address) is unique on one
//interface.
//
//We can have the same L3/L4 IDs on multiple nodes. On one node, the same
//address can have multiple L3/L4 IDs (two mf823 for example). However, same
//address (+prefix) and same interface is guaranteed to have unique L3/L4
#define UPDATE_UPDATE       "UPDATE NetworkUpdates SET " \
                            "Timestamp=?,EventValueStr=? " \
                            "WHERE "\
                            "L3SessionId=? AND L4SessionId=? " \
                            "AND NetworkAddress=? AND InterfaceId=?"

#define UPDATE_EVENT_ID     "UPDATE NetworkEvent SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_UPDATES_ID   "UPDATE NetworkUpdates SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define DELETE_TABLE        "DELETE FROM NetworkEvent"

#define DELETE_GPS_TABLE    "DELETE FROM GpsEvents"

//This statement is a static version of what the .dump command does. A dynamic
//version would query the master table to get tables and then PRAGMA to get
//rows. The actualy query is simpler than it looks. It seems SQLite supports
//prepending/appending random text to the result of a query, and that is what we
//do here. quote() gives a quoted string of the row content, suitable for
//inclusion in another SQL query. The || is string concation. So, the query
//queries for all columns, and each row is prefixed with some string
#define DUMP_EVENTS         "SELECT \"INSERT IGNORE INTO NetworkEvent"\
                            "(NodeId,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventType,EventParam,"\
                            "EventValue,EventValueStr,InterfaceType,"\
                            "InterfaceIdType,InterfaceId,"\
                            "NetworkProvider,NetworkAddressFamily,"\
                            "NetworkAddress) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"), quote(\"EventType\"), "\
                            "quote(\"EventParam\"), quote(\"EventValue\"), "\
                            "quote(\"EventValueStr\"), "\
                            "quote(\"InterfaceType\"), quote(\"InterfaceIdType\"), "\
                            "quote(\"InterfaceId\"),"\
                            "quote(\"NetworkProvider\"), quote(\"NetworkAddressFamily\"), "\
                            "quote(\"NetworkAddress\") || \")\" FROM  \"NetworkEvent\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define DUMP_UPDATES        "SELECT \"REPLACE INTO NetworkUpdate"\
                            "(NodeId,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventValueStr,InterfaceType, InterfaceId,"\
                            "NetworkAddress,NetworkProvider,ServerTimestamp) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"),quote(\"EventValueStr\"), "\
                            "quote(\"InterfaceType\"), quote(\"InterfaceId\"),"\
                            "quote(\"NetworkAddress\"),quote(\"NetworkProvider\") || \",Now())\""\
                            "FROM \"NetworkUpdates\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define INSERT_GPS_EVENT    "INSERT INTO GpsEvents(NodeId,Timestamp" \
                            ",Sequence,Latitude,Longitude,Altitude" \
                            ",GroundSpeed,NumOfSatelites) " \
                            "VALUES (?,?,?,?,?,?,?,?)"


#define DUMP_GPS            "SELECT \"REPLACE INTO GpsEvents" \
                            "(NodeId,Timestamp,Sequence,Latitude,Longitude" \
                            ",Altitude,GroundSpeed,NumOfSatelites) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"Latitude\"), "\
                            "quote(\"Longitude\"), quote(\"Altitude\"), "\
                            "quote(\"GroundSpeed\"), quote(\"NumOfSatelites\") "\
                            "|| \")\" FROM \"GpsEvents\" ORDER BY Timestamp;"

struct md_event;
struct md_writer;
struct backend_timeout_handle;

struct md_writer_sqlite {
    MD_WRITER;

    sqlite3 *db_handle;

    sqlite3_stmt *insert_provider, *insert_update;
    sqlite3_stmt *update_update, *dump_update;
    sqlite3_stmt *delete_table, *dump_table;
    sqlite3_stmt *last_update;

    sqlite3_stmt *insert_gps, *delete_gps, *dump_gps;

    uint32_t node_id;
    uint32_t db_interval;
    uint32_t db_events;
    uint32_t num_conn_events;
    uint32_t num_gps_events;

    uint8_t timeout_added;
    uint8_t file_failed;
    uint8_t do_fake_updates;
    struct timeval first_fake_update;

    uint64_t dump_tstamp;
    uint64_t last_msg_tstamp;

    float gps_speed;

    struct backend_timeout_handle *timeout_handle;
    char meta_prefix[128], gps_prefix[128];
    size_t meta_prefix_len, gps_prefix_len;
};

void md_sqlite_setup(struct md_exporter *mde, struct md_writer_sqlite* mws);

#endif
