/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MTPDEVICE_H
#define MTPDEVICE_H

#include "device.h"
#include "song.h"
#include <solid/portablemediaplayer.h>

// class KJob;

class MtpDevice : public Device
{
    Q_OBJECT

public:
    MtpDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~MtpDevice();

    void connectTo();
    void disconnectFrom();
    bool isConnected();
    void rescan();
    bool isRefreshing() const { return false; }// return 0!=scanner; }
    void configure(QWidget *parent);
    QString path() { return QString(); } // audioFolder; }
    void addSong(const Song &s, bool overwrite);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite);
    void removeSong(const Song &s);
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();

private:
    void setup();
    void startScanner();
    void stopScanner();

private Q_SLOTS:
    void libraryUpdated();
//     void saveProperties(const QString &newPath, const Device::NameOptions &opts);
//     void addSongResult(KJob *job);
//     void copySongToResult(KJob *job);
//     void removeSongResult(KJob *job);

private:
    Solid::PortableMediaPlayer *pmp;
};

#endif
