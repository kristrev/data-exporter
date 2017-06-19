#include <string.h>
#include <stdint.h>

#include "metadata_writer_inventory_system.h"
#include "metadata_exporter_log.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_writer_json_helpers.h"

uint8_t md_inventory_handle_system_event(struct md_writer_sqlite *mws,
                                         md_system_event_t *mse)
{
    sqlite3_stmt *stmt = mws->insert_system;
    sqlite3_clear_bindings(stmt);
    sqlite3_reset(stmt);

    if (sqlite3_bind_int(stmt, 1, mws->node_id) ||
        sqlite3_bind_int(stmt, 2, mws->session_id) ||          // BootCount
        sqlite3_bind_int(stmt, 3, mws->session_id_multip) ||   // BootMultiplier
        sqlite3_bind_int(stmt, 4, mse->tstamp) ||
        sqlite3_bind_int(stmt, 5, mse->sequence) ||
        sqlite3_bind_text(stmt, 6, mse->imei, strlen(mse->imei),
                          SQLITE_STATIC)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to bind values to "
                          "INSERT query (system)\n");
        return RETVAL_FAILURE;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        return RETVAL_FAILURE;
    } else {
        return RETVAL_SUCCESS;
    }
}

static uint8_t md_inventory_system_dump_db_json(struct md_writer_sqlite *mws,
        FILE *output)
{
    const char *json_str;
    sqlite3_reset(mws->dump_gps);

    json_object *jarray = json_object_new_array();

    if (md_json_helpers_dump_write(mws->dump_system, jarray))
    {
        json_object_put(jarray);
        return RETVAL_FAILURE;
    }

    json_str = json_object_to_json_string_ext(jarray, JSON_C_TO_STRING_PLAIN);
    fprintf(output, "%s", json_str);

    json_object_put(jarray);
    return RETVAL_SUCCESS;
}

static uint8_t md_inventory_system_delete_db(struct md_writer_sqlite *mws)
{
    int32_t retval;

    sqlite3_reset(mws->delete_system);
    retval = sqlite3_step(mws->delete_system);

    if (retval != SQLITE_DONE) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failed to delete system %s\n",
                sqlite3_errstr(retval));
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}

uint8_t md_inventory_system_copy_db(struct md_writer_sqlite *mws)
{
    uint8_t retval = RETVAL_SUCCESS;

    retval = md_writer_helpers_copy_db(mws->system_prefix,
            mws->system_prefix_len, md_inventory_system_dump_db_json, mws,
            md_inventory_system_delete_db);

    if (retval == RETVAL_SUCCESS)
        mws->num_system_events = 0;

    return retval;
}
