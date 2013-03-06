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

#ifndef CDPARANOIA_H
#define CDPARANOIA_H

#include <QString>

extern "C" {
#include <cdda_interface.h>
#include <cdda_paranoia.h>
}

class CdParanoia
{
public:
    CdParanoia();
    ~CdParanoia();

    bool setDevice(const QString &device);
    void setParanoiaMode(int mode);
    void setNeverSkip(bool b);
    void setMaxRetries(int m) { maxRetries=m; }

    qint16 * read();
    int seek(long sector, int mode);

    int firstSectorOfTrack(int track);
    int lastSectorOfTrack(int track);
    int firstSectorOfDisc();
    int lastSectorOfDisc();
    int length();

    int lengthOfTrack(int n);
    int numOfFramesOfTrack(int n);
    double sizeOfTrack(int n); //in MiB
    int frameOffsetOfTrack(int n);
    bool isAudioTrack(int n);
    void reset();

private:
    bool init();
    void free();

private:
    QString dev;
    cdrom_drive *drive;
    cdrom_paranoia *paranoia;
    int paranoiaMode;
    bool neverSkip;
    int maxRetries;
};

#endif
