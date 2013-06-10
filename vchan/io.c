/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk      <rafal@invisiblethingslab.com>
 * Copyright (C) 2013  Marek Marczykowski  <marmarek@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <xenstore.h>
#include "libvchan.h"
#include "libvchan_private.h"

extern xc_interface *xc_handle;

int libvchan_write(libvchan_t *ctrl, const void *data, size_t size) {
    return libxenvchan_write(ctrl->xenvchan, (char*)data, size);
}

int libvchan_send(libvchan_t *ctrl, void *data, size_t size) {
    return libxenvchan_send(ctrl->xenvchan, (char*)data, size);
}

int libvchan_read(libvchan_t *ctrl, void *data, size_t size) {
    return libxenvchan_read(ctrl->xenvchan, (char*)data, size);
}

int libvchan_recv(libvchan_t *ctrl, void *data, size_t size) {
    return libxenvchan_recv(ctrl->xenvchan, (char*)data, size);
}

int libvchan_wait(libvchan_t *ctrl) {
    return libxenvchan_wait(ctrl->xenvchan);
}

void libvchan_close(libvchan_t *ctrl) {
    struct xs_handle *xs;

    libxenvchan_close(ctrl->xenvchan);
    if (ctrl->xs_path) {
        /* remove xenstore entry */
        xs = xs_open(0);
        if (xs) {
            /* if xenstore connection failed just do not remove entries, but do
             * not abort whole function, especially still free the memory
             */
            xs_rm(xs, 0, ctrl->xs_path);
            xs_close(xs);
        }
        free(ctrl->xs_path);
    }
    free(ctrl);
}

EVTCHN libvchan_fd_for_select(libvchan_t *ctrl) {
    /* TODO: Windows */
    return libxenvchan_fd_for_select(ctrl->xenvchan);
}

int libvchan_data_ready(libvchan_t *ctrl) {
    return libxenvchan_data_ready(ctrl->xenvchan);
}

int libvchan_buffer_space(libvchan_t *ctrl) {
    return libxenvchan_buffer_space(ctrl->xenvchan);
}

int libvchan_is_open(libvchan_t *ctrl) {
    int ret;
    struct evtchn_status evst;

    ret =  libxenvchan_is_open(ctrl->xenvchan);
    if (!ret)
        return ret;
    /* slow check in case of domain destroy */
    evst.port = ctrl->xenvchan->event_port;
    evst.dom = DOMID_SELF;
    if (xc_evtchn_status(xc_handle, &evst)) {
        perror("xc_evtchn_status");
        return 0;
    }
    if (evst.status != EVTCHNSTAT_interdomain) {
        if (!ctrl->xenvchan->is_server)
            ctrl->xenvchan->ring->srv_live = 0;
        return 0;
    }
    return ret;
}
