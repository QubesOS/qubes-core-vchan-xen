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

libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min) {
    char xs_path[255];
    libvchan_t *ctrl;

    snprintf(xs_path, sizeof(xs_path), "data/vchan/%d/%d", domain, port);
    ctrl = libxenvchan_server_init(NULL, domain, xs_path, read_min, write_min);
    if (!ctrl)
        return NULL;
    ctrl->blocking = 1;
    return ctrl;
}

#ifndef WINNT

char *libvchan_get_domain_name(int domain) {
    char xs_path[255];
    struct xs_handle *xs;
    unsigned int len = 0;
    char *name;

    snprintf(xs_path, sizeof(xs_path), "/local/domain/%d/name", domain);
    xs = xs_daemon_open();
    if (!xs) {
        perror("xs_daemon_open");
        return NULL;
    }
    name = xs_read(xs, 0, xs_path, &len);
    if (!name) {
        perror("xs_read");
        return NULL;
    }
    xs_daemon_close(xs);
    return name;
}

libvchan_t *libvchan_client_init(int domain, int port) {
    char xs_path[255];
    libvchan_t *ctrl;
    struct xs_handle *xs;
    char **vec;
    unsigned int count, len;
    char *dummy;

    /* FIXME: get own domain ID instead of hardcoded "0" here */
    snprintf(xs_path, sizeof(xs_path), "/local/domain/%d/data/vchan/0/%d", domain, port);

    /* wait for server to appear */
    xs = xs_open(0);
    if (!xs) {
        perror("xs_open");
        return NULL;
    }
    xs_watch(xs, xs_path, xs_path);
    do {
        vec = xs_read_watch(xs, &count);
        if (vec)
            free(vec);
        dummy = xs_read(xs, 0, xs_path, &len);
    } while (!dummy);
    free(dummy);
    xs_close(xs);

    ctrl = libxenvchan_client_init(NULL, domain, xs_path);
    if (!ctrl)
        return NULL;
    ctrl->blocking = 1;
    return ctrl;
}

#else

// Client side not implemented on Windows domains yet

libvchan_t *libvchan_client_init(int domain, int port) {
	return NULL;
}

char *libvchan_get_domain_name(int domain) {
	return NULL;
}

#endif
