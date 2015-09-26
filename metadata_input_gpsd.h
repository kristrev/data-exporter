#ifndef METADATA_INPUT_GPSD_H
#define METADATA_INPUT_GPSD_H

#include <gps.h>

#include "metadata_exporter.h"

struct backend_epoll_handle;

struct md_input_gpsd {
    MD_INPUT;
    struct gps_data_t gps_data;
    struct backend_epoll_handle *event_handle;
};

void md_gpsd_setup(struct md_exporter *mde, struct md_input_gpsd *mig);

#endif
