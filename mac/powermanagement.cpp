/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "powermanagement.h"
#include "support/globalstatic.h"
#include "mpd-interface/mpdstatus.h"
#include <Carbon/Carbon.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

GLOBAL_STATIC(PowerManagement, instance)

static void powerEventCallback(void *rootPort, io_service_t, natural_t msg, void *arg)
{
    switch (msg) {
    case kIOMessageSystemWillSleep:
        IOAllowPowerChange(*(io_connect_t *) rootPort, (long)arg);
        break;
    case kIOMessageSystemHasPoweredOn:
        PowerManagement::self()->emitResuming();
        break;
    case kIOMessageCanSystemSleep:
        if (PowerManagement::self()->inhibitSuspend() && MPDState_Playing==MPDStatus::self()->state()) {
            IOCancelPowerChange(*(io_connect_t *) rootPort, (long)arg);
        } else {
            IOAllowPowerChange(*(io_connect_t *) rootPort, (long)arg);
        }
        break;
    default:
        break;
    }
}

PowerManagement::PowerManagement()
    : inhibitSuspendWhilstPlaying(false)
{
    static io_connect_t rootPort;
    IONotificationPortRef notificationPort;
    io_object_t notifier;

    rootPort = IORegisterForSystemPower(&rootPort, &notificationPort, powerEventCallback, &notifier);
    if (rootPort) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notificationPort), kCFRunLoopDefaultMode);
    }
}

void PowerManagement::emitResuming()
{
    emit resuming();
}

#include "moc_powermanagement.cpp"
