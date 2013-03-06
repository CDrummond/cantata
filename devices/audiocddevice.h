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

#ifndef AUDIOCDDEVICE_H
#define AUDIOCDDEVICE_H

#include "device.h"
#ifdef ENABLE_KDE_SUPPORT
#include <solid/block.h>
#include <solid/opticaldisc.h>
#include <solid/opticaldrive.h>
#else
#include "solid-lite/block.h"
#include "solid-lite/opticaldisc.h"
#include "solid-lite/opticaldrive.h"
#endif

class Cddb;
class CddbAlbum;

class AudioCdDevice : public Device
{
    Q_OBJECT

public:
    AudioCdDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~AudioCdDevice();

    bool supportsDisconnect() const { return 0!=drive; }
    bool isConnected() const { return 0!=block; }
    QString icon() const { return "media-optical"; }
    void rescan(bool);
    bool isRefreshing() const { return lookupInProcess; }
    void toggle();
    void stop();
    QString path() const { return QString(); } // audioFolder; }
    void addSong(const Song &, bool, bool) { }
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover);
    void removeSong(const Song &) { }
    void cleanDirs(const QSet<QString> &) { }
    double usedCapacity() { return 1.0; }
    QString capacityString() { return detailsString; }
    qint64 freeSpace() { return 1.0; }
    DevType devType() const { return AudioCd; }
    void saveOptions() { }
    QString subText() { return albumName; }
    quint32 totalTime();

Q_SIGNALS:
    void lookup();
    void matches(const QString &u, const QList<CddbAlbum> &);

public Q_SLOTS:
    void percent(int pc);
    void copySongToResult(int status);
    void setDetails(const CddbAlbum &a);
    void cddbMatches(const QList<CddbAlbum> &albums);

private:
    Solid::OpticalDisc *disc;
    Solid::OpticalDrive *drive;
    Solid::Block *block;
    Cddb *cddb;
    QString detailsString;
    QString albumName;
    quint32 time;
    bool lookupInProcess;
};

#endif
