/*
 * drive-status.h - Lightweight drive status introspection helpers.
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

#ifndef VICE_DRIVE_STATUS_H
#define VICE_DRIVE_STATUS_H

#include "drive.h"

typedef struct {
    int drive_num;
    int motor_on;
    int led_on;
    int track;
    int rw_mode;
    int step_event;
} drive_status_t;

void drive_status_init(void);
void drive_status_reset_unit(unsigned int unit);
void drive_status_set_motor(unsigned int unit, int motor_on);
void drive_status_set_step_event(unsigned int unit);
int drive_status_get(unsigned int unit, drive_status_t *status, int clear_step);
int drive_status_unit_active(unsigned int unit);
int drive_status_drive_to_unit(int drive_num);

#endif
