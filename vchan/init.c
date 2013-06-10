/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
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

/* intentionally use common xc_interface for all libvchan connections, it
 * doesn't hold any connection-specific informations; it is used only in
 * libvchan_is_open
 */
xc_interface *xc_handle = NULL;

libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min) {
    char xs_path[255];
    libvchan_t *ctrl;

    if (!xc_handle) {
        xc_handle = xc_interface_open(NULL, NULL, 0);
        if (!xc_handle) {
            /* error already logged by xc_interface_open */
            return NULL;
        }
    }

    snprintf(xs_path, sizeof(xs_path), "data/vchan/%d/%d", domain, port);
    ctrl = libxenvchan_server_init(NULL, domain, xs_path, read_min, write_min);
    if (!ctrl)
        return NULL;
    ctrl->blocking = 1;
    return ctrl;
}

#ifndef WINNT

libvchan_t *libvchan_client_init(int domain, int port) {
    char xs_path[255];
    char xs_path_watch[255];
    libvchan_t *ctrl;
    struct xs_handle *xs;
    char **vec;
    unsigned int count, len;
    char *dummy;
    char *own_domid;

    if (!xc_handle) {
        xc_handle = xc_interface_open(NULL, NULL, 0);
        if (!xc_handle) {
            /* error already logged by xc_interface_open */
            return NULL;
        }
    }

    /* wait for server to appear */
    xs = xs_open(0);
    if (!xs) {
        perror("xs_open");
        return NULL;
    }

    own_domid = xs_read(xs, 0, "domid", &len);
    if (!own_domid) {
        fprintf(stderr, "Cannot get own domid\n");
        xs_close(xs);
        return NULL;
    }

    snprintf(xs_path, sizeof(xs_path), "/local/domain/%d/data/vchan/%s/%d",
            domain, own_domid, port);
    /* watch on this key as we might not have access to whole directory */
    snprintf(xs_path_watch, sizeof(xs_path_watch), "%s/event-channel", xs_path);

    xs_watch(xs, xs_path_watch, xs_path_watch);
    do {
        vec = xs_read_watch(xs, &count);
        if (vec)
            free(vec);
        dummy = xs_read(xs, 0, xs_path_watch, &len);
    } while (!dummy);
    free(dummy);
    free(own_domid);
    xs_close(xs);

    ctrl = libxenvchan_client_init(NULL, domain, xs_path);
    if (!ctrl)
        return NULL;
    ctrl->blocking = 1;
    /* notify server */
    xc_evtchn_notify(ctrl->event, ctrl->event_port);
    return ctrl;
}

#else

// Client side not implemented on Windows domains yet

libvchan_t *libvchan_client_init(int domain, int port) {
	return NULL;
}

#endif
