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
#include "covers.h"
#ifdef ENABLE_KDE_SUPPORT
#include <solid/block.h>
#include <solid/opticaldrive.h>
#else
#include "solid-lite/block.h"
#include "solid-lite/opticaldrive.h"
#endif
#include <QImage>

class Cddb;
class MusicBrainz;
class CdAlbum;

class AudioCdDevice : public Device
{
    Q_OBJECT

public:
    enum Service {
        SrvNone,
        SrvCddb,
        SrvMusicBrainz
    };

    static QString coverUrl(QString udi);

    AudioCdDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~AudioCdDevice();

    bool supportsDisconnect() const { return 0!=drive; }
    bool isConnected() const { return 0!=block; }
    QString icon() const { return "media-optical"; }
    void rescan(bool);
    bool isRefreshing() const { return lookupInProcess; }
    void toggle();
    void stop();
    QString path() const { return devPath; }
    void addSong(const Song &, bool, bool) { }
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover);
    void removeSong(const Song &) { }
    void cleanDirs(const QSet<QString> &) { }
    double usedCapacity() { return 1.0; }
    QString capacityString() { return detailsString; }
    qint64 freeSpace() { return 1.0; }
    DevType devType() const { return AudioCd; }
    void saveOptions() { }
    QString subText() { return album; }
    quint32 totalTime();
    #ifdef CDDB_PLAYBACK
    bool canPlaySongs() const { return true; }
    #endif

    QString albumName() const { return album; }
    QString albumArtist() const { return artist; }
    QString albumGenre() const { return genre; }
    int albumDisc() const { return disc; }
    int albumYear() const { return year; }
    const Covers::Image & cover() const { return coverImage; }

Q_SIGNALS:
    void lookup();
    void matches(const QString &u, const QList<CdAlbum> &);

public Q_SLOTS:
    void percent(int pc);
    void copySongToResult(int status);
    void setDetails(const CdAlbum &a);
    void cdMatches(const QList<CdAlbum> &albums);
    void setCover(const Song &song, const QImage &img, const QString &file);

private:
    void connectService();

private:
    Service srv;
    Solid::OpticalDrive *drive;
    Solid::Block *block;
    #ifdef CDDB_FOUND
    Cddb *cddb;
    #endif
    #ifdef MUSICBRAINZ5_FOUND
    MusicBrainz *mb;
    #endif
    QString detailsString;
    QString album;
    QString artist;
    QString genre;
    QString devPath;
    int year;
    int disc;
    quint32 time;
    bool lookupInProcess;
    Covers::Image coverImage;
};

#endif
