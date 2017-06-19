#include "metadata_writer_inventory_system.h"

uint8_t md_inventory_handle_system_event(struct md_writer_sqlite *mws,
                                         md_system_event_t *mse)
{
    sqlite3_stmt *stmt = mws->insert_gps;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

#if 0
    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int(stmt, 2, mws->session_id) ||          // BootCount
        sqlite3_bind_int(stmt, 3, mws->session_id_multip) ||   // BootMultiplier
        sqlite3_bind_int(stmt, 4, mge->tstamp_tv.tv_sec) ||
        sqlite3_bind_int(stmt, 5, mge->sequence) ||
        sqlite3_bind_int(stmt, 6, mge->md_type) ||
        sqlite3_bind_int(stmt, 7, 0) ||
        sqlite3_bind_double(stmt, 8, mge->latitude) ||
        sqlite3_bind_double(stmt, 9, mge->longitude)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to INSERT query (GPS)\n");
        return RETVAL_FAILURE;
    }

    if (mge->altitude &&
        sqlite3_bind_double(stmt, 10, mge->altitude)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind altitude\n");
        return RETVAL_FAILURE;
    }

    if (mws->gps_speed &&
        sqlite3_bind_double(stmt, 11, mws->gps_speed)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind speed\n");
        return RETVAL_FAILURE;
    }

    if (mge->satellites_tracked &&
        sqlite3_bind_int(stmt, 12, mge->satellites_tracked)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind num. satelites\n");
        return RETVAL_FAILURE;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        return RETVAL_FAILURE;
    } else {
        mws->last_gps_insert = mge->tstamp_tv.tv_sec;
        return RETVAL_SUCCESS;
    }
#endif
    return RETVAL_SUCCESS;
}

uint8_t md_inventory_system_copy_db(struct md_writer_sqlite *mws)
{

    return RETVAL_SUCCESS;
}
