/*
 * monitor_drivestatus_server.c - Lightweight drive status TCP server.
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>
#include <string.h>

#include "cmdline.h"
#include "drive-status.h"
#include "lib.h"
#include "log.h"
#include "monitor_drivestatus_server.h"
#include "resources.h"
#include "util.h"
#include "vicesocket.h"

#ifdef HAVE_NETWORK

static vice_network_socket_t *listen_socket = NULL;
static vice_network_socket_t *client_socket = NULL;
static char *server_address = NULL;
static int server_enabled = 0;

static drive_status_t prev_status[NUM_DISK_UNITS];
static int prev_valid[NUM_DISK_UNITS];

static void drivestatus_server_reset_prev(void)
{
    unsigned int i;
    for (i = 0; i < NUM_DISK_UNITS; i++) {
        prev_valid[i] = 0;
    }
}

static void drivestatus_server_close_client(void)
{
    if (client_socket) {
        vice_network_socket_close(client_socket);
        client_socket = NULL;
    }
    drivestatus_server_reset_prev();
}

static void drivestatus_server_send_line(const drive_status_t *status)
{
    char line[128];

    sprintf(line, "%d %d %d %d %d %d\n",
            status->drive_num,
            status->motor_on,
            status->led_on,
            status->track,
            status->rw_mode,
            status->step_event);
    vice_network_send(client_socket, line, strlen(line), 0);
}

static void drivestatus_server_send_error(void)
{
    const char *err = "ERROR: INVALID DRIVE\n";
    vice_network_send(client_socket, err, strlen(err), 0);
}

static void drivestatus_server_send_initial(void)
{
    unsigned int unit;
    int any = 0;

    for (unit = 0; unit < NUM_DISK_UNITS; unit++) {
        drive_status_t status;
        if (drive_status_get(unit, &status, 1) != 0) {
            continue;
        }
        any = 1;
        drivestatus_server_send_line(&status);
        prev_status[unit] = status;
        prev_status[unit].step_event = 0; /* consumed */
        prev_valid[unit] = 1;
    }

    if (!any) {
        drivestatus_server_send_error();
    }
}

static void drivestatus_server_poll_listen(void)
{
    vice_network_socket_t *client;

    if (!listen_socket) {
        return;
    }

    if (!vice_network_select_poll_one(listen_socket)) {
        return;
    }

    client = vice_network_accept(listen_socket);
    if (!client) {
        return;
    }

    /* Only one client at a time: drop previous */
    drivestatus_server_close_client();
    client_socket = client;
    drivestatus_server_reset_prev();
    drivestatus_server_send_initial();
}

void monitor_drivestatus_poll(void)
{
    if (!server_enabled) {
        return;
    }

    drivestatus_server_poll_listen();

    if (client_socket) {
        /* check if client hung up */
        if (vice_network_select_poll_one(client_socket)) {
            char tmp[4];
            ssize_t r = vice_network_receive(client_socket, tmp, sizeof tmp, 0);
            if (r <= 0) {
                drivestatus_server_close_client();
                return;
            }
        }

        /* push changes */
        unsigned int unit;
        for (unit = 0; unit < NUM_DISK_UNITS; unit++) {
            drive_status_t status;
            if (drive_status_get(unit, &status, 0) != 0) {
                if (prev_valid[unit]) {
                    drivestatus_server_send_error();
                    prev_valid[unit] = 0;
                }
                continue;
            }

            if (!prev_valid[unit]
                || memcmp(&prev_status[unit], &status, sizeof status) != 0) {
                drivestatus_server_send_line(&status);

                /* clear step_event once delivered */
                if (status.step_event) {
                    drive_status_get(unit, &status, 1);
                    status.step_event = 0;
                }
                prev_status[unit] = status;
                prev_valid[unit] = 1;
            }
        }
    }
}

static int drivestatus_server_activate(void)
{
    vice_network_socket_address_t *addr = NULL;
    int error = 1;

    do {
        if (!server_address) {
            log_error(LOG_DEFAULT, "drivestatus server address not set");
            break;
        }

        addr = vice_network_address_generate(server_address, 0);
        if (!addr) {
            log_error(LOG_DEFAULT, "drivestatus server address invalid");
            break;
        }

        listen_socket = vice_network_server(addr);
        if (!listen_socket) {
            log_error(LOG_DEFAULT, "could not start drivestatus server socket");
            break;
        }

        error = 0;
    } while (0);

    if (addr) {
        vice_network_address_close(addr);
    }

    return error;
}

static void drivestatus_server_deactivate(void)
{
    if (listen_socket) {
        vice_network_socket_close(listen_socket);
        listen_socket = NULL;
    }
}

static int set_server_enabled(int value, void *param)
{
    int val = value ? 1 : 0;

    if (!val) {
        if (server_enabled) {
            drivestatus_server_deactivate();
        }
        server_enabled = 0;
        return 0;
    }

    if (!server_enabled) {
        if (drivestatus_server_activate() < 0) {
            return -1;
        }
    }
    server_enabled = 1;
    return 0;
}

static int set_server_address(const char *name, void *param)
{
    if (server_address != NULL && name != NULL
        && strcmp(name, server_address) == 0) {
        return 0;
    }

    if (server_enabled) {
        drivestatus_server_deactivate();
    }

    util_string_set(&server_address, name);

    if (server_enabled) {
        return drivestatus_server_activate();
    }

    return 0;
}

static const resource_string_t drivestatus_resources_string[] = {
    { "DriveStatusServerAddress", "ip4://127.0.0.1:6511", RES_EVENT_NO, NULL,
      &server_address, set_server_address, NULL },
    RESOURCE_STRING_LIST_END
};

static const resource_int_t drivestatus_resources_int[] = {
    { "DriveStatusServer", 0, RES_EVENT_STRICT, (resource_value_t)0,
      &server_enabled, set_server_enabled, NULL },
    RESOURCE_INT_LIST_END
};

int monitor_drivestatus_resources_init(void)
{
    if (resources_register_string(drivestatus_resources_string) < 0) {
        return -1;
    }

    return resources_register_int(drivestatus_resources_int);
}

void monitor_drivestatus_resources_shutdown(void)
{
    drivestatus_server_deactivate();
    lib_free(server_address);
    server_address = NULL;
}

static const cmdline_option_t drivestatus_cmdline_options[] = {
    { "-drivestatusserver", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DriveStatusServer", (resource_value_t)1,
      NULL, "Enable drive status TCP server" },
    { "+drivestatusserver", SET_RESOURCE, CMDLINE_ATTRIB_NONE,
      NULL, NULL, "DriveStatusServer", (resource_value_t)0,
      NULL, "Disable drive status TCP server" },
    { "-drivestatusaddress", SET_RESOURCE, CMDLINE_ATTRIB_NEED_ARGS,
      NULL, NULL, "DriveStatusServerAddress", NULL,
      "<addr>", "Bind drive status TCP server to address" },
    CMDLINE_LIST_END
};

int monitor_drivestatus_cmdline_options_init(void)
{
    return cmdline_register_options(drivestatus_cmdline_options);
}

#else

int monitor_drivestatus_resources_init(void)
{
    return 0;
}

void monitor_drivestatus_resources_shutdown(void)
{
}

int monitor_drivestatus_cmdline_options_init(void)
{
    return 0;
}

void monitor_drivestatus_poll(void)
{
}

#endif
