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

#ifndef DEVICE_H
#define DEVICE_H

#include "musiclibraryitemroot.h"
#include "song.h"
#include <QtCore/QObject>
#include <solid/device.h>

class QWidget;
class DevicesModel;

class Device : public MusicLibraryItemRoot
{
    Q_OBJECT

public:

    struct NameOptions {
        static const QLatin1String constAlbumArtist;
        static const QLatin1String constAlbumTitle;
        static const QLatin1String constTrackArtist;
        static const QLatin1String constTrackTitle;
        static const QLatin1String constTrackNumber;
        static const QLatin1String constCdNumber;
        static const QLatin1String constGenre;
        static const QLatin1String constYear;

        NameOptions();

        bool operator==(const NameOptions &o) const {
            return vfatSafe==o.vfatSafe && asciiOnly==o.asciiOnly && ignoreThe==o.ignoreThe && replaceSpaces==o.replaceSpaces && scheme==o.scheme;
        }
        bool operator!=(const NameOptions &o) const {
            return !(*this==o);
        }
        QString clean(const QString &str) const;
        Song clean(const Song &s) const;
        QString createFilename(const Song &s) const;

        QString scheme;
        bool vfatSafe;
        bool asciiOnly;
        bool ignoreThe;
        bool replaceSpaces;
    };

    static Device * create(DevicesModel *m, const QString &udi);

    enum Status {
        Ok,
        FileExists,
        SongExists,
        DirCreationFaild,
        SourceFileDoesNotExist,
        Failed,
        NotConnected
    };

    Device(DevicesModel *m, Solid::Device &dev)
        : MusicLibraryItemRoot(dev.vendor()+QChar(' ')+dev.product())
        , model(m)
        , solidDev(dev)
        , update(0) {
    }
    virtual ~Device() {
    }

    QString icon() const {
        return solidDev.icon();
    }

    virtual void connectTo()=0;
    virtual void disconnectFrom()=0;
    virtual bool isConnected()=0;
    virtual void rescan()=0;
    virtual bool isRefreshing() const=0;
    virtual void configure(QWidget *parent)=0;
    virtual QString path()=0;
    virtual void addSong(const Song &s, bool overwrite)=0;
    virtual void copySongTo(const Song &s, const QString &fullDest, bool overwrite)=0;
    virtual void removeSong(const Song &s)=0;
    virtual double usedCapacity()=0;
    virtual QString capacityString()=0;
    virtual qint64 freeSpace()=0;

    const Solid::Device & dev() const {
        return solidDev;
    }

    void applyUpdate();
    bool haveUpdate() const {
        return 0!=update;
    }
    int newRows() const {
        return update ? update->childCount() : 0;
    }
    const NameOptions & namingOptions() const {
        return nameOpts;
    }

    bool songExists(const Song &s) const;

protected:
    void addSongToList(const Song &s);
    void removeSongFromList(const Song &s);

Q_SIGNALS:
    void connected(const QString &udi);
    void disconnected(const QString &udi);
    void updating(const QString &udi, bool s);
    void actionStatus(int);

protected:
    DevicesModel *model;
    NameOptions nameOpts;
    Solid::Device solidDev;
    MusicLibraryItemRoot *update;
    Song currentSong;
};

#endif
