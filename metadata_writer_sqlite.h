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

#pragma once

#include <sys/time.h>
#include <sqlite3.h>
#include "metadata_exporter.h"

#define DEFAULT_TIMEOUT 5000
#define TIMEOUT_FILE 1000
#define EVENT_LIMIT 10
#define MAX_PATH_LEN 128
#define FAKE_UPDATE_LIMIT 120

//This the first valid timestamp of an event and the value is not randomly
//chosen, it is the time when this piece of code was written. And
//since time is never supposed to move backwards ... Note that this check
//assumes that all nodes will have some offset time lower than this, and
//then ntp (or something else) will set a correct time. A good starting
//point is epoch
#define FIRST_VALID_TIMESTAMP 1455740094

#define CREATE_SQL          "CREATE TABLE IF NOT EXISTS NetworkEvent(" \
                            "NodeId INTEGER NOT NULL," \
                            "SessionId INTEGER NOT NULL," \
                            "SessionIdMultip INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER NOT NULL DEFAULT 0," \
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
                            "PRIMARY KEY(SessionId,SessionIdMultip,Timestamp,"\
                            "Sequence))"

#define CREATE_UPDATE_SQL   "CREATE TABLE IF NOT EXISTS NetworkUpdates(" \
                            "NodeId INTEGER NOT NULL," \
                            "SessionId INTEGER NOT NULL," \
                            "SessionIdMultip INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "L3SessionId INTEGER NOT NULL," \
                            "L4SessionId INTEGER NOT NULL DEFAULT 0," \
                            "EventValueStr TEXT NOT NULL," \
                            "InterfaceType INTEGER NOT NULL," \
                            "InterfaceId TEXT NOT NULL," \
                            "NetworkAddress TEXT NOT NULL," \
                            "NetworkProvider INT," \
                            "PRIMARY KEY(SessionId,SessionIdMultip,"\
                            "L3SessionId,L4SessionId,InterfaceId,"\
                            "NetworkAddress))"

#define CREATE_GPS_SQL      "CREATE TABLE IF NOT EXISTS GpsUpdate(" \
                            "NodeId INTEGER NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "Sequence INTEGER NOT NULL," \
                            "Latitude REAL NOT NULL," \
                            "Longitude REAL NOT NULL," \
                            "Altitude REAL," \
                            "GroundSpeed REAL," \
                            "NumOfSatelites INTEGER," \
                            "PRIMARY KEY(NodeId) ON CONFLICT REPLACE)"

#define CREATE_MONITOR_SQL  "CREATE TABLE IF NOT EXISTS MonitorEvents(" \
                            "NodeId      INTEGER NOT NULL," \
                            "Timestamp   INTEGER NOT NULL," \
                            "Sequence    INTEGER NOT NULL," \
                            "Boottime    INTEGER NOT NULL," \
                            "PRIMARY KEY(NodeId,Timestamp,Sequence))"

#define CREATE_USAGE_SQL    "CREATE TABLE IF NOT EXISTS DataUse(" \
                            "DeviceId TEXT NOT NULL," \
                            "SimCardIccid TEXT NOT NULL," \
                            "SimCardImsi TEXT NOT NULL," \
                            "Timestamp INTEGER NOT NULL," \
                            "RxData INTEGER NOT NULL,"\
                            "TxData INTEGER NOT NULL," \
                            "PRIMARY KEY(DeviceId,SimCardIccid,SimCardImsi,Timestamp))"

#define INSERT_EVENT        "INSERT INTO NetworkEvent(NodeId,SessionId,"\
                            "SessionIdMultip,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventType,EventParam,EventValue,"\
                            "EventValueStr,InterfaceType,InterfaceIdType,"\
                            "InterfaceId,NetworkProvider,NetworkAddressFamily,"\
                            "NetworkAddress) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_UPDATE       "INSERT INTO NetworkUpdates(NodeId,SessionId,"\
                            "SessionIdMultip,Timestamp,Sequence,L3SessionId,"\
                            "L4SessionId,EventValueStr,InterfaceType,"\
                            "InterfaceId,NetworkAddress,NetworkProvider) " \
                            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"

#define INSERT_GPS_EVENT    "INSERT INTO GpsUpdate(NodeId,Timestamp" \
                            ",Sequence,Latitude,Longitude,Altitude" \
                            ",GroundSpeed,NumOfSatelites) " \
                            "VALUES (?,?,?,?,?,?,?,?)"

#define INSERT_MONITOR_EVENT "INSERT INTO MonitorEvents(NodeId,Timestamp" \
                             ",Sequence,Boottime) " \
                             "VALUES (?,?,?,?)"

#define INSERT_USAGE        "INSERT INTO DataUse(DeviceId,SimCardIccid" \
                            ",SimCardImsi,Timestamp,RxData,TxData) " \
                            "VALUES (?,?,?,?,?,?)"

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

#define UPDATE_USAGE        "UPDATE DataUse SET " \
                            "RxData = RxData + ?, TxData = TxData + ? " \
                            "WHERE " \
                            "DeviceId=? AND SimCardIccid=? AND SimCardImsi=? AND Timestamp=?"

#define UPDATE_EVENT_ID     "UPDATE NetworkEvent SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_UPDATES_ID   "UPDATE NetworkUpdates SET " \
                            "NodeId=? "\
                            "WHERE NodeId=0"

#define UPDATE_EVENT_TSTAMP "UPDATE NetworkEvent SET " \
                            "Timestamp = Timestamp + ? "\
                            "WHERE Timestamp < ?"

#define UPDATE_UPDATES_TSTAMP     "UPDATE NetworkUpdates SET " \
                                  "Timestamp = Timestamp + ? "\
                                  "WHERE Timestamp < ?"

#define UPDATE_EVENT_SESSION_ID "UPDATE NetworkEvent SET "\
                                "SessionId=?,SessionIdMultip=? "\
                                "WHERE SessionId = 0"

#define UPDATE_UPDATES_SESSION_ID "UPDATE NetworkUpdates SET "\
                                  "SessionId=?,SessionIdMultip=? "\
                                  "WHERE SessionId = 0"

#define DELETE_TABLE         "DELETE FROM NetworkEvent"

#define DELETE_GPS_TABLE     "DELETE FROM GpsUpdate"

#define DELETE_MONITOR_TABLE "DELETE FROM MonitorEvents"

#define DELETE_USAGE_TABLE "DELETE FROM DataUse"

//This statement is a static version of what the .dump command does. A dynamic
//version would query the master table to get tables and then PRAGMA to get
//rows. The actualy query is simpler than it looks. It seems SQLite supports
//prepending/appending random text to the result of a query, and that is what we
//do here. quote() gives a quoted string of the row content, suitable for
//inclusion in another SQL query. The || is string concation. So, the query
//queries for all columns, and each row is prefixed with some string
#define DUMP_EVENTS         "SELECT \"INSERT IGNORE INTO NetworkEventV2"\
                            "(NodeId,SessionId,SessionIdMultip,Timestamp,"\
                            "Sequence,L3SessionId,L4SessionId,EventType,"\
                            "EventParam,EventValue,EventValueStr,"\
                            "InterfaceType,InterfaceIdType,InterfaceId,"\
                            "NetworkProvider,NetworkAddressFamily,"\
                            "NetworkAddress) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"SessionId\"),"\
                            "quote(\"SessionIdMultip\"),quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"), quote(\"EventType\"), "\
                            "quote(\"EventParam\"), quote(\"EventValue\"), "\
                            "quote(\"EventValueStr\"), "\
                            "quote(\"InterfaceType\"), quote(\"InterfaceIdType\"), "\
                            "quote(\"InterfaceId\"),"\
                            "quote(\"NetworkProvider\"), quote(\"NetworkAddressFamily\"), "\
                            "quote(\"NetworkAddress\") || \")\" FROM  \"NetworkEvent\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define DUMP_UPDATES        "SELECT \"REPLACE INTO NetworkUpdateV2"\
                            "(NodeId,SessionId,SessionIdMultip,Timestamp,"\
                            "Sequence,L3SessionId,L4SessionId,EventValueStr,"\
                            "InterfaceType, InterfaceId,NetworkAddress,"\
                            "NetworkProvider,ServerTimestamp) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"SessionId\"),"\
                            "quote(\"SessionIdMultip\"),quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"),quote(\"EventValueStr\"), "\
                            "quote(\"InterfaceType\"), quote(\"InterfaceId\"),"\
                            "quote(\"NetworkAddress\"),quote(\"NetworkProvider\") || \",Now())\""\
                            "FROM \"NetworkUpdates\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define DUMP_EVENTS_V2      "SELECT \"INSERT IGNORE INTO NetworkEvent"\
                            "(NodeId,BootCount,BootMultiplier,Timestamp,"\
                            "Sequence,L3SessionId,L4SessionId,DeviceId,"\
                            "DeviceTypeId,NetworkAddress,NetworkAddressFamily,"\
                            "EventType,EventParam,EventValue,EventValueStr,Operator) "\
                            "VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"SessionId\"),"\
                            "quote(\"SessionIdMultip\"),quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"), quote(\"InterfaceId\"), "\
                            "quote(\"InterfaceType\"), quote(\"NetworkAddress\"), "\
                            "quote(\"NetworkAddressFamily\"), quote(\"EventType\"), "\
                            "quote(\"EventParam\"), quote(\"EventValue\"), "\
                            "quote(\"EventValueStr\"), quote(\"NetworkProvider\") "\
                            "|| \")\" FROM  \"NetworkEvent\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define DUMP_UPDATES_V2     "SELECT \"REPLACE INTO NetworkUpdate"\
                            "(NodeId,BootCount,BootMultiplier,Timestamp,"\
                            "Sequence,L3SessionId,L4SessionId,EventValueStr,"\
                            "DeviceTypeId, DeviceId,NetworkAddress,"\
                            "Operator,ServerTimestamp) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"SessionId\"),"\
                            "quote(\"SessionIdMultip\"),quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"L3SessionId\"), "\
                            "quote(\"L4SessionId\"),quote(\"EventValueStr\"), "\
                            "quote(\"InterfaceType\"), quote(\"InterfaceId\"),"\
                            "quote(\"NetworkAddress\"),quote(\"NetworkProvider\") || \",Now())\""\
                            "FROM \"NetworkUpdates\" WHERE Timestamp>=? ORDER BY Timestamp;"

#define DUMP_GPS            "SELECT \"REPLACE INTO GpsUpdates" \
                            "(NodeId,Timestamp,Sequence,Latitude,Longitude" \
                            ",Altitude,Speed,SatelliteCount) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"Latitude\"), "\
                            "quote(\"Longitude\"), quote(\"Altitude\"), "\
                            "quote(\"GroundSpeed\"), quote(\"NumOfSatelites\") "\
                            "|| \")\" FROM \"GpsUpdate\" ORDER BY Timestamp;"

#define DUMP_MONITOR        "SELECT \"REPLACE INTO MonitorEvents" \
                            "(NodeId,Timestamp,Sequence,Boottime) VALUES(\" "\
                            "|| quote(\"NodeId\"), quote(\"Timestamp\"), "\
                            "quote(\"Sequence\"), quote(\"Boottime\") "\
                            "|| \")\" FROM \"MonitorEvents\" ORDER BY Timestamp;"

#define DUMP_USAGE          "SELECT \"INSERT INTO DataUse" \
                            "(DeviceId,SimCardIccid,SimCardImsi,IntervalStart,RxData,TxData) VALUES(\" "\
                            "|| quote(\"DeviceId\"), quote(\"SimCardIccid\"), " \
                            "quote(\"SimCardImsi\") || \",FROM_UNIXTIME(\" "\
                            "|| quote(\"Timestamp\") || \"),\" || "\
                            "quote(\"RxData\"), quote(\"TxData\") "\
                            "|| \") ON DUPLICATE KEY UPDATE RxData=RxData+\"" \
                            "|| quote(\"RxData\") || \", TxData=TxData+\"" \
                            "|| quote(\"TxData\") FROM \"DataUse\";"

struct md_event;
struct md_writer;
struct backend_timeout_handle;

struct md_writer_sqlite {
    MD_WRITER;

    sqlite3 *db_handle;

    sqlite3_stmt *insert_event, *insert_update;
    sqlite3_stmt *update_update, *dump_update;
    sqlite3_stmt *delete_table, *dump_table;
    sqlite3_stmt *last_update;

    sqlite3_stmt *insert_gps, *delete_gps, *dump_gps;
    sqlite3_stmt *insert_monitor, *delete_monitor, *dump_monitor;

    sqlite3_stmt *insert_usage, *update_usage, *dump_usage, *delete_usage;

    const char *session_id_file;

    uint32_t node_id;
    uint32_t db_interval;
    uint32_t db_events;
    uint32_t num_conn_events;
    uint32_t num_gps_events;
    uint32_t num_munin_events;
    uint32_t num_usage_events;

    uint8_t timeout_added;
    uint8_t file_failed;
    uint8_t do_fake_updates;
    uint8_t valid_timestamp;
    struct timeval first_fake_update;

    uint64_t dump_tstamp;
    uint64_t last_msg_tstamp;
    uint64_t last_gps_insert;

    //TODO: Consider moving this to the generic writer struct if need be
    //These values keep track of the unique session id (and multiplier), which
    //are normally assumed to be the boot counter (+ multiplier)
    uint64_t session_id;
    uint64_t session_id_multip;

    float gps_speed;

    struct backend_timeout_handle *timeout_handle;
    char   meta_prefix[128], gps_prefix[128], monitor_prefix[128], usage_prefix[128];
    size_t meta_prefix_len,  gps_prefix_len,  monitor_prefix_len, usage_prefix_len;

    uint8_t api_version;
};

void md_sqlite_usage();
void md_sqlite_setup(struct md_exporter *mde, struct md_writer_sqlite* mws);
