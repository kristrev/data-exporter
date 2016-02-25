#pragma once

#include <uv.h>

#include "metadata_exporter.h"

struct backend_epoll_handle;

struct md_input_sysevent {
    MD_INPUT;
    struct backend_epoll_handle *event_handle;
    void* responder;
    char* message;
    int zmq_fd;
};

void md_sysevent_setup(struct md_exporter *mde, struct md_input_sysevent *mis);
