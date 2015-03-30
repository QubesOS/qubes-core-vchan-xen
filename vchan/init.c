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
#include <string.h>
#include <xenstore.h>
#include "libvchan.h"
#include "libvchan_private.h"

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

    ctrl = malloc(sizeof(*ctrl));
    if (!ctrl)
        return NULL;

    snprintf(xs_path, sizeof(xs_path), "data/vchan/%d/%d", domain, port);
    ctrl->xenvchan = libxenvchan_server_init(NULL, domain, xs_path, read_min, write_min);
    if (!ctrl->xenvchan) {
        free(ctrl);
        return NULL;
    }
    ctrl->xs_path = strdup(xs_path);
    ctrl->xenvchan->blocking = 1;
    return ctrl;
}

#ifndef WINNT

libvchan_t *libvchan_client_init(int domain, int port) {
    char xs_path[255];
    char xs_path_dom[255];
    char xs_path_watch[255];
    libvchan_t *ctrl;
    struct xs_handle *xs;
    char **vec;
    unsigned int count, len;
    char *dummy, *dummy2;
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

    own_domid = NULL;

    if (!xs_watch(xs, "domid", "domid")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        xs_close(xs);
        return NULL;
    }
    if (!xs_watch(xs, "@releaseDomain", "release")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        xs_close(xs);
        return NULL;
    }
    do {
        if (!own_domid) {
            /* construct xenstore path on first iteration and on every domid
             * change detected (save+restore case */
            own_domid = xs_read(xs, 0, "domid", &len);
            if (!own_domid) {
                fprintf(stderr, "Cannot get own domid\n");
                xs_close(xs);
                return NULL;
            }
            if (atoi(own_domid) == domain) {
                fprintf(stderr, "Loopback vchan connection not supported\n");
                free(own_domid);
                xs_close(xs);
                return NULL;
            }

            snprintf(xs_path_dom, sizeof(xs_path_dom), "/local/domain/%d",
                    domain);
            snprintf(xs_path, sizeof(xs_path), "/local/domain/%d/data/vchan/%s/%d",
                    domain, own_domid, port);
            /* watch on this key as we might not have access to whole directory */
            snprintf(xs_path_watch, sizeof(xs_path_watch), "%s/event-channel", xs_path);

            if (!xs_watch(xs, xs_path_watch, xs_path_watch)) {
                fprintf(stderr, "Cannot setup watch on %s\n", xs_path_watch);
                free(own_domid);
                xs_close(xs);
                return NULL;
            }
        }
        vec = xs_read_watch(xs, &count);
        if (vec) {
            if (strcmp(vec[XS_WATCH_TOKEN], "domid") == 0) {
                /* domid have changed -> reread it on next iteration */
                free(own_domid);
                own_domid = NULL;
                xs_unwatch(xs, xs_path_watch, xs_path_watch);
            }
            free(vec);
        }
        dummy = xs_read(xs, 0, xs_path_watch, &len);
        if (dummy)
            free(dummy);
        else {
            if (own_domid && strcmp(own_domid, "0")==0) {
                /* check if domain still alive */
                dummy2 = xs_read(xs, 0, xs_path_dom, &len);
                if (!dummy2) {
                    fprintf(stderr, "domain dead\n");
                    xs_close(xs);
                    return NULL;
                }
                free(dummy2);
            } else {
                /* FIXME: find a way to check if remote domain is still alive from domU */
            }
        }
    } while (!dummy || !len);
    free(own_domid);
    xs_close(xs);

    ctrl = malloc(sizeof(*ctrl));
    if (!ctrl)
        return NULL;
    ctrl->xs_path = NULL;
    ctrl->xenvchan = libxenvchan_client_init(NULL, domain, xs_path);
    if (!ctrl->xenvchan) {
        free(ctrl);
        return NULL;
    }
    ctrl->xenvchan->blocking = 1;
    /* notify server */
    xc_evtchn_notify(ctrl->xenvchan->event, ctrl->xenvchan->event_port);
    return ctrl;
}

#else

// Client side not implemented on Windows domains yet

libvchan_t *libvchan_client_init(int domain, int port) {
	return NULL;
}

#endif
