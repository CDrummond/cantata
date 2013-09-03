/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mpdstatus.h"
#include "mpdconnection.h"
#ifndef Q_OS_WIN32
#include "powermanagement.h"
#endif

MPDStatus * MPDStatus::self()
{
    static MPDStatus instance;
    return &instance;
}

#ifndef Q_OS_WIN32
bool MPDStatus::inhibitSuspend()
{
    return inhibitSuspendWhilstPlaying;
}

void MPDStatus::setInhibitSuspend(bool i)
{
    if (i!=inhibitSuspendWhilstPlaying) {
        inhibitSuspendWhilstPlaying=i;
        if (!inhibitSuspendWhilstPlaying) {
            PowerManagement::self()->stopSuppressingSleep();
        } else if (inhibitSuspendWhilstPlaying && (MPDState_Playing==values.state)) {
            PowerManagement::self()->beginSuppressingSleep();
        }
    }
}
#endif

MPDStatus::MPDStatus()
    #ifndef Q_OS_WIN32
    : inhibitSuspendWhilstPlaying(false)
    #endif
{
    connect(MPDConnection::self(), SIGNAL(statusUpdated(const MPDStatusValues &)), this, SLOT(update(const MPDStatusValues &)), Qt::QueuedConnection);
}

void MPDStatus::update(const MPDStatusValues &v)
{
    #ifndef Q_OS_WIN32
    if (inhibitSuspendWhilstPlaying) {
        if (MPDState_Playing==v.state) {
            PowerManagement::self()->beginSuppressingSleep();
        } else {
            PowerManagement::self()->stopSuppressingSleep();
        }
    }
    #endif
    values=v;
    emit updated();
}
