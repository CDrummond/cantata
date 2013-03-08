/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

CdParanoia::CdParanoia()
    : drive(0)
    , paranoia(0)
    , paranoiaMode(0)
    , neverSkip(true)
    , maxRetries(20)
{
    paranoia = 0;
    drive = 0;
    setNeverSkip(true);
    setMaxRetries(20);
    setParanoiaMode(3);
}

CdParanoia::~CdParanoia()
{
    free();
}

bool CdParanoia::setDevice(const QString& device)
{
    dev = device;
    return init();
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
        paranoia_modeset(paranoia, paranoiaMode);
    }
}

void CdParanoia::setNeverSkip(bool b)
{
    neverSkip = b;
    setParanoiaMode(paranoiaMode);
}

qint16 * CdParanoia::read()
{
    return paranoia ? paranoia_read_limited(paranoia, 0, maxRetries) : 0;
}

int CdParanoia::seek(long sector, int mode)
{
    return paranoia ? paranoia_seek(paranoia, sector, mode) : -1;
}

int CdParanoia::firstSectorOfTrack(int track)
{
    return paranoia ? cdda_track_firstsector(drive, track) : -1;
}

int CdParanoia::lastSectorOfTrack(int track)
{
    return paranoia ? cdda_track_lastsector(drive, track) : -1;
}

int CdParanoia::firstSectorOfDisc()
{
    return paranoia ? cdda_disc_firstsector(drive) : -1;
}

int CdParanoia::lastSectorOfDisc()
{
    return paranoia ? cdda_disc_lastsector(drive) : -1;
}

void CdParanoia::reset()
{
    init();
}

bool CdParanoia::init()
{
    free();
    drive = cdda_identify(dev.toLatin1().data(), 0, 0);
    if (!drive) {
        return false;
    }

    cdda_open(drive);
    paranoia = paranoia_init(drive);
    if (paranoia == 0) {
        free();
        return false;
    }

    return true;
}

void CdParanoia::free() {
    if (paranoia) {
        paranoia_free(paranoia);
        paranoia = 0;
    }
    if (drive) {
        cdda_close(drive);
        drive = 0;
    }
}
