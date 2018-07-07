/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2013  Marek Marczykowski <marmarek@invisiblethingslab.com>
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

#ifndef _LIBVCHAN_PRIVATE_H
#define _LIBVCHAN_PRIVATE_H

#include <libxenvchan.h>

#if _MSC_VER < 1900
#define snprintf _snprintf
#endif

struct libvchan {
    struct libxenvchan *xenvchan;
    /* store path, which should be removed after client connect (server only) */
    char *xs_path;
    int remote_domain;
};

int libvchan__check_domain_alive(HANDLE xc_handle, int dom);
void _Log(XENCONTROL_LOG_LEVEL logLevel, PCHAR function, PWCHAR format, ...);

#define Log(level, msg, ...) _Log(level, __FUNCTION__, L"(%p)" L##msg L"\n", ctrl, __VA_ARGS__)

#endif
