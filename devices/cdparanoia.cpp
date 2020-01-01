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

#include "cdparanoia.h"
#include "config.h"
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

static QSet<QString> lockedDevices;
static QMutex mutex;

CdParanoia::CdParanoia(const QString &device, bool full, bool noSkip, bool playback, int offset)
    : drive(0)
    , paranoia(0)
    , paranoiaMode(0)
    , neverSkip(noSkip)
    , maxRetries(20)
    , seekOffst(offset)
{
    QMutexLocker locker(&mutex);
    if (!lockedDevices.contains(device)) {
        dev = device;
        if (init()) {
            lockedDevices.insert(device);
        } else {
            dev=QString();
        }
    }

    if (!dev.isEmpty()) {
        setFullParanoiaMode(full);
        if (playback) {
            maxRetries=1;
            #if !defined CDIOPARANOIA_FOUND && defined CDPARANOIA_HAS_CACHEMODEL_SIZE
            paranoia_cachemodel_size(paranoia, 24);
            #endif
        }
    }
}

CdParanoia::~CdParanoia()
{
    QMutexLocker locker(&mutex);
    free();
    if (!dev.isEmpty()) {
        lockedDevices.remove(dev);
    }
}

void CdParanoia::setParanoiaMode(int mode)
{
    // from cdrdao 1.1.7
    paranoiaMode = PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP;

    switch (mode) {
    case 0: paranoiaMode = PARANOIA_MODE_DISABLE; break;
    case 1: paranoiaMode |= PARANOIA_MODE_OVERLAP; paranoiaMode &= ~PARANOIA_MODE_VERIFY; break;
    case 2: paranoiaMode &= ~(PARANOIA_MODE_SCRATCH|PARANOIA_MODE_REPAIR); break;
    }

    if (neverSkip) {
        paranoiaMode |= PARANOIA_MODE_NEVERSKIP;
    }

    if (paranoia) {
        #ifdef CDIOPARANOIA_FOUND
        cdio_paranoia_modeset(paranoia, paranoiaMode);
        #else
        paranoia_modeset(paranoia, paranoiaMode);
        #endif
    }
}

qint16 * CdParanoia::read()
{
    #ifdef CDIOPARANOIA_FOUND
    return paranoia ? cdio_paranoia_read_limited(paranoia, 0, maxRetries) : 0;
    #else
    return paranoia ? paranoia_read_limited(paranoia, 0, maxRetries) : 0;
    #endif
}

int CdParanoia::seek(long sector, int mode)
{
    #ifdef CDIOPARANOIA_FOUND
    return paranoia ? cdio_paranoia_seek(paranoia, sector+seekOffst, mode) : -1;
    #else
    return paranoia ? paranoia_seek(paranoia, sector+seekOffst, mode) : -1;
    #endif
}

int CdParanoia::firstSectorOfTrack(int track)
{
    #ifdef CDIOPARANOIA_FOUND
    return paranoia ? cdio_cddap_track_firstsector(drive, track) : -1;
    #else
    return paranoia ? cdda_track_firstsector(drive, track) : -1;
    #endif
}

int CdParanoia::lastSectorOfTrack(int track)
{
    #ifdef CDIOPARANOIA_FOUND
    return paranoia ? cdio_cddap_track_lastsector(drive, track) : -1;
    #else
    return paranoia ? cdda_track_lastsector(drive, track) : -1;
    #endif
}

bool CdParanoia::init()
{
    free();
    #ifdef CDIOPARANOIA_FOUND
    drive = cdda_identify(dev.toLatin1().data(), 0, 0);
    #else
    drive = cdda_identify(dev.toLatin1().data(), 0, 0);
    #endif
    if (!drive) {
        return false;
    }

    #ifdef CDIOPARANOIA_FOUND
    cdio_cddap_open(drive);
    paranoia = cdio_paranoia_init(drive);
    #else
    cdda_open(drive);
    paranoia = paranoia_init(drive);
    #endif
    if (paranoia == 0) {
        free();
        return false;
    }

    return true;
}

void CdParanoia::free()
{
    if (paranoia) {
        #ifdef CDIOPARANOIA_FOUND
        cdio_paranoia_free(paranoia);
        #else
        paranoia_free(paranoia);
        #endif
        paranoia = 0;
    }
    if (drive) {
        #ifdef CDIOPARANOIA_FOUND
        cdio_cddap_close(drive);
        #else
        cdda_close(drive);
        #endif
        drive = 0;
    }
}
