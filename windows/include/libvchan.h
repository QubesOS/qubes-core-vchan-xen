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

#ifndef _LIBVCHAN_H
#define _LIBVCHAN_H

#include <windows.h>
typedef HANDLE EVTCHN;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LIBVCHAN_EXPORTS
#    define LIBVCHAN_API __declspec(dllexport)
#else
#    define LIBVCHAN_API __declspec(dllimport)
#endif

/* config vchan features */
#define QREXEC_RING_V2
#define ASYNC_INIT

#ifdef CONFIG_STUBDOM
#define ASYNC_INIT
#endif /* CONFIG_STUBDOM */

/* return values from libvchan_is_open */
/* remote disconnected or remote domain dead */
#define VCHAN_DISCONNECTED 0
/* connected */
#define VCHAN_CONNECTED 1
/* vchan server initialized, waiting for client to connect */
#define VCHAN_WAITING 2

struct libvchan;
typedef struct libvchan libvchan_t;

typedef void libvchan_logger_t(IN int logLevel, IN const char *function, IN const WCHAR *format, IN va_list args);

LIBVCHAN_API
void libvchan_register_logger(libvchan_logger_t *logger);

/*
Note: libvchan_*_init sets last error to ERROR_NOT_SUPPORTED if
the xeniface device is not available. The caller can potentially
wait for xeniface to become active in that case (this can happen
after the first reboot after pvdrivers installation, xeniface takes
a while to load).
*/

LIBVCHAN_API
libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min);

LIBVCHAN_API
libvchan_t *libvchan_client_init(int domain, int port);

LIBVCHAN_API
int libvchan_write(libvchan_t *ctrl, const void *data, size_t size);

LIBVCHAN_API
int libvchan_send(libvchan_t *ctrl, const void *data, size_t size);

LIBVCHAN_API
int libvchan_read(libvchan_t *ctrl, void *data, size_t size);

LIBVCHAN_API
int libvchan_recv(libvchan_t *ctrl, void *data, size_t size);

LIBVCHAN_API
int libvchan_wait(libvchan_t *ctrl);

LIBVCHAN_API
void libvchan_close(libvchan_t *ctrl);

LIBVCHAN_API
EVTCHN libvchan_fd_for_select(libvchan_t *ctrl);

LIBVCHAN_API
int libvchan_is_open(libvchan_t *ctrl);

LIBVCHAN_API
int libvchan_data_ready(libvchan_t *ctrl);

LIBVCHAN_API
int libvchan_buffer_space(libvchan_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* _LIBVCHAN_H */
