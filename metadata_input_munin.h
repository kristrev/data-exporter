#ifndef METADATA_INPUT_MUNIN_H
#define METADATA_INPUT_MUNIN_H

#include <uv.h>

#include "metadata_exporter.h"

struct backend_epoll_handle;

struct md_input_munin {
    MD_INPUT;
    struct backend_epoll_handle *event_handle;
    int munin_socket_fd;
    int module_count; 
    char** modules; 
};

void md_munin_setup(struct md_exporter *mde, struct md_input_munin *mim);

#endif
