/*
 * drive-status.c - Lightweight drive status introspection helpers.
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

#include "drive-status.h"

static int drive_status_motor[NUM_DISK_UNITS];
static int drive_status_step[NUM_DISK_UNITS];

void drive_status_init(void)
{
    unsigned int i;

    for (i = 0; i < NUM_DISK_UNITS; i++) {
        drive_status_reset_unit(i);
    }
}

void drive_status_reset_unit(unsigned int unit)
{
    if (unit >= NUM_DISK_UNITS) {
        return;
    }

    drive_status_motor[unit] = -1;
    drive_status_step[unit] = 0;
}

void drive_status_set_motor(unsigned int unit, int motor_on)
{
    if (unit >= NUM_DISK_UNITS) {
        return;
    }

    drive_status_motor[unit] = motor_on ? 1 : 0;
}

void drive_status_set_step_event(unsigned int unit)
{
    if (unit >= NUM_DISK_UNITS) {
        return;
    }

    drive_status_step[unit] = 1;
}

int drive_status_drive_to_unit(int drive_num)
{
    if (drive_num < DRIVE_UNIT_MIN || drive_num > DRIVE_UNIT_MAX) {
        return -1;
    }

    return drive_num - DRIVE_UNIT_MIN;
}

int drive_status_unit_active(unsigned int unit)
{
    if (unit >= NUM_DISK_UNITS) {
        return 0;
    }

    if (diskunit_context[unit] == NULL) {
        return 0;
    }

    return diskunit_context[unit]->enable && diskunit_context[unit]->type != DRIVE_TYPE_NONE;
}

int drive_status_get(unsigned int unit, drive_status_t *status, int clear_step)
{
    drive_t *drive;
    int motor_on;

    if (status == NULL || unit >= NUM_DISK_UNITS) {
        return -1;
    }

    if (!drive_status_unit_active(unit)) {
        return -1;
    }

    drive = diskunit_context[unit]->drives[0];

    status->drive_num = DRIVE_UNIT_MIN + unit;

    motor_on = drive_status_motor[unit];
    if (motor_on < 0) {
        motor_on = (drive->byte_ready_active & BRA_MOTOR_ON) ? 1 : 0;
        drive_status_motor[unit] = motor_on;
    }
    status->motor_on = motor_on;

    status->led_on = (drive->led_status & 1) ? 1 : 0;
    status->track = (drive->current_half_track > 0)
        ? ((drive->current_half_track + 1) / 2)
        : 0;

    if (status->motor_on == 0) {
        status->rw_mode = 0;
    } else if (drive->read_write_mode) {
        status->rw_mode = 1;
    } else {
        status->rw_mode = 2;
    }

    /* step_event acts as a one-shot flag that can be cleared on read. */
    status->step_event = drive_status_step[unit];
    if (clear_step) {
        drive_status_step[unit] = 0;
    }

    return 0;
}
