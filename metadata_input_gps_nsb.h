#ifndef METADATA_INPUT_GPS_NSB_H
#define METADATA_INPUT_GPS_NSB_H

#include "metadata_exporter.h"

struct backend_epoll_handle;

struct md_input_gps_nsb {
    MD_INPUT;
    struct backend_epoll_handle *event_handle;
};

void md_gps_nsb_setup(struct md_exporter *mde, struct md_input_gps_nsb *mign);

#endif
