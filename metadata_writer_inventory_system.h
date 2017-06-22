#ifndef METADATA_WRITER_INVENTORY_SYSTEM_H
#define METADATA_WRITER_INVENTORY_SYSTEM_H

#include "metadata_exporter.h"
#include "metadata_writer_sqlite.h"

uint8_t md_inventory_handle_system_event(struct md_writer_sqlite *mws,
                                         md_system_event_t *mse);
uint8_t md_inventory_system_copy_db(struct md_writer_sqlite *mws);

#endif
