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
#define XC_WANT_COMPAT_EVTCHN_API

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenstore.h>
#include <xenctrl.h>
#include "libvchan.h"
#include "libvchan_private.h"

static domid_t parse_domid(const char *ptr) {
    char *endptr;
    unsigned long ret;

    if (!ptr)
        return DOMID_INVALID;
    if (ptr[0] == '0')
        return ptr[1] ? DOMID_INVALID : 0;
    if (ptr[0] < '1' || ptr[0] > '9')
        return DOMID_INVALID;
    ret = strtoul(ptr, &endptr, 10);
    return (*endptr || ret >= DOMID_FIRST_RESERVED) ? DOMID_INVALID : (domid_t)ret;
}

libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min) {
    libvchan_t *ctrl;

    ctrl = calloc(sizeof(*ctrl), 1);
    if (!ctrl)
        return NULL;

    if (asprintf(&ctrl->xs_path, "data/vchan/%d/%d", domain, port) < 0)
        goto err_asprintf;
    ctrl->xenvchan = libxenvchan_server_init(NULL, domain, ctrl->xs_path, read_min, write_min);
    if (!ctrl->xenvchan)
        goto err_server_init;
    ctrl->xenvchan->blocking = 1;
    ctrl->remote_domain = domain;
    if (!(ctrl->xc_handle = xc_interface_open(NULL, NULL, 0)))
        goto err_xc_open;
    return ctrl;
err_xc_open:
    libxenvchan_close(ctrl->xenvchan);
err_server_init:
    free(ctrl->xs_path);
err_asprintf:
    free(ctrl);
    return NULL;
}

libvchan_t *libvchan_client_init(int domain, int port) {
    char xs_path[255];
    char xs_path_watch[255];
    libvchan_t *ctrl;
    xc_interface *xc_handle;
    struct xs_handle *xs;
    char **vec;
    unsigned int count, len;
    char *dummy = NULL;
    char *own_domid = NULL;

    if (domain < 0 || (unsigned)domain >= DOMID_FIRST_RESERVED) {
        fprintf(stderr, "Invalid peer domain ID %d\n", domain);
        return NULL;
    }

    if (port < 0) {
        fprintf(stderr, "Invalid port %d\n", port);
        return NULL;
    }

    xc_handle = xc_interface_open(NULL, NULL, 0);
    if (!xc_handle) {
        /* error already logged by xc_interface_open */
        goto err;
    }

    /* wait for server to appear */
    xs = xs_open(0);
    if (!xs) {
        perror("xs_open");
        goto err_xc;
    }

    len = 0;

    if (!xs_watch(xs, "domid", "domid")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        goto err_xs;
    }
    if (!xs_watch(xs, "@releaseDomain", "release")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        goto err_xs;
    }
    while (!dummy || !len) {
        vec = xs_read_watch(xs, &count);
        if (vec) {
            if (strcmp(vec[XS_WATCH_TOKEN], "domid") == 0) {
                /* domid have changed */
                if (own_domid) {
                    free(own_domid);
                    own_domid = NULL;
                    xs_unwatch(xs, xs_path_watch, xs_path_watch);
                }
            }
            free(vec);
        }
        if (!own_domid) {
            int v;

            /* construct xenstore path on first iteration and on every domid
             * change detected (save+restore case) */
            own_domid = xs_read(xs, 0, "domid", &len);
            if (!own_domid) {
                fprintf(stderr, "Cannot get own domid\n");
                goto err_xs;
            }
            int own_domid_num = parse_domid(own_domid);
            if (own_domid_num == DOMID_INVALID) {
                fprintf(stderr, "Invalid own domid %s\n", own_domid);
                free(own_domid);
                goto err_xs;
            }
            if (own_domid_num == domain) {
                fprintf(stderr, "Loopback vchan connection not supported\n");
                free(own_domid);
                goto err_xs;
            }

            v = snprintf(xs_path, sizeof(xs_path), "/local/domain/%d/data/vchan/%s/%d",
                         domain, own_domid, port);
            if ((unsigned)v >= sizeof(xs_path)) {
                free(own_domid);
                goto err_xs;
            }
            /* watch on this key as we might not have access to the whole directory */
            v = snprintf(xs_path_watch, sizeof(xs_path_watch), "%.128s/event-channel", xs_path);
            if ((unsigned)v >= sizeof(xs_path_watch)) {
                free(own_domid);
                goto err_xs;
            }

            if (!xs_watch(xs, xs_path_watch, xs_path_watch)) {
                fprintf(stderr, "Cannot setup watch on %s\n", xs_path_watch);
                free(own_domid);
                goto err_xs;
            }
        }

        dummy = xs_read(xs, 0, xs_path_watch, &len);
        if (dummy)
            free(dummy);
        else {
            if (!libvchan__check_domain_alive(xc_handle, domain)) {
                fprintf(stderr, "domain dead\n");
                goto err_xs;
            }
        }
    }

    free(own_domid);
    xs_close(xs);

    ctrl = malloc(sizeof(*ctrl));
    if (!ctrl)
        goto err_xc;
    ctrl->xs_path = NULL;
    ctrl->xenvchan = libxenvchan_client_init(NULL, domain, xs_path);
    if (!ctrl->xenvchan) {
        free(ctrl);
        goto err_xc;
    }
    ctrl->xenvchan->blocking = 1;
    /* notify server */
    xc_evtchn_notify(ctrl->xenvchan->event, ctrl->xenvchan->event_port);
    ctrl->remote_domain = domain;
    ctrl->xc_handle = xc_handle;
    return ctrl;

err_xs:
    xs_close(xs);
err_xc:
    xc_interface_close(xc_handle);
err:
    return NULL;
}
