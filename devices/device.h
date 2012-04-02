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
#include "encoders.h"
#include <QtCore/QObject>
#include <solid/device.h>

class QWidget;
class DevicesModel;

class Device : public QObject, public MusicLibraryItemRoot
{
    Q_OBJECT

public:

    struct Options {
        static const QLatin1String constAlbumArtist;
        static const QLatin1String constAlbumTitle;
        static const QLatin1String constTrackArtist;
        static const QLatin1String constTrackTitle;
        static const QLatin1String constTrackNumber;
        static const QLatin1String constCdNumber;
        static const QLatin1String constGenre;
        static const QLatin1String constYear;

        Options();

        void load(const QString &group);
        void save(const QString &group);

        bool operator==(const Options &o) const {
            return vfatSafe==o.vfatSafe && asciiOnly==o.asciiOnly && ignoreThe==o.ignoreThe &&
                   replaceSpaces==o.replaceSpaces && scheme==o.scheme && fixVariousArtists==o.fixVariousArtists &&
                   useCache==o.useCache && transcoderCodec==o.transcoderCodec &&
                   (transcoderCodec.isEmpty() || (transcoderValue==o.transcoderValue && transcoderWhenDifferent==o.transcoderWhenDifferent));
        }
        bool operator!=(const Options &o) const {
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
        bool fixVariousArtists;
        QString transcoderCodec;
        int transcoderValue;
        bool transcoderWhenDifferent;
        bool useCache;
    };

    static Device * create(DevicesModel *m, const QString &udi);
    static bool fixVariousArtists(const QString &file, Song &song, bool applyFix);

    static const QLatin1String constNoCover;

    enum Status {
        Ok,
        FileExists,
        SongExists,
        DirCreationFaild,
        SourceFileDoesNotExist,
        SongDoesNotExist,
        Failed,
        NotConnected,
        CodecNotAvailable,
        TranscodeFailed,
        FailedToCreateTempFile
    };

    enum DevType {
        Ums,
        Mtp,
        Remote
    };

    Device(DevicesModel *m, Solid::Device &dev)
        : MusicLibraryItemRoot(dev.vendor()+QChar(' ')+dev.product())
        , model(m)
        , configured(false)
        , solidDev(dev)
        , update(0)
        , needToFixVa(false)
        , jobAbortRequested(false) {
    }
    Device(DevicesModel *m, const QString &name)
        : MusicLibraryItemRoot(name)
        , model(m)
        , configured(false)
        , update(0)
        , needToFixVa(false)
        , jobAbortRequested(false) {
    }
    virtual ~Device() {
    }

    virtual QString icon() const {
        return solidDev.icon();
    }
    virtual QString coverFile() const {
        return QString();
    }

    virtual bool isConnected() const=0;
    virtual void rescan()=0;
    virtual bool isRefreshing() const=0;
    bool isIdle() const { return isConnected() && !isRefreshing(); }
    virtual void configure(QWidget *) { }
    virtual QString path() const =0;
    virtual void addSong(const Song &s, bool overwrite)=0;
    virtual void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)=0;
    virtual void removeSong(const Song &s)=0;
    virtual void cleanDir(const QString &dir)=0;
    virtual double usedCapacity()=0;
    virtual QString capacityString()=0;
    virtual qint64 freeSpace()=0;
    virtual DevType devType() const=0;
    virtual void saveCache() {
    }
    virtual void removeCache() {
    }

    virtual QString udi() const {
        return solidDev.udi();
    }
    void applyUpdate();
    bool haveUpdate() const {
        return 0!=update;
    }
    int newRows() const {
        return update ? update->childCount() : 0;
    }
    const Options & options() const {
        return opts;
    }
    void setOptions(const Options &o) {
        opts=o;
        saveOptions();
    }
    virtual void saveOptions()=0;
    const QString & statusMessage() const {
        return statusMsg;
    }
    bool isDevice() const {
        return true;
    }
    const MusicLibraryItem * findSong(const Song &s) const;
    bool songExists(const Song &s) const;
    bool isConfigured() {
        return configured;
    }

    void abortJob() {
        jobAbortRequested=true;
    }
    bool abortRequested() const {
        return jobAbortRequested;
    }
    virtual bool canPlaySongs() const {
        return false;
    }

public Q_SLOTS:
    void setStatusMessage(const QString &message);
    void songCount(int c);

public:
    void addSongToList(const Song &s);
    void removeSongFromList(const Song &s);

Q_SIGNALS:
    void connected(const QString &udi);
    void disconnected(const QString &udi);
    void updating(const QString &udi, bool s);
    void actionStatus(int);
    void progress(unsigned long percent);
    void error(const QString &);

protected:
    DevicesModel *model;
    Options opts;
    bool configured;
    Solid::Device solidDev;
    MusicLibraryItemRoot *update;
    Song currentSong;
    QString currentBaseDir;
    QString currentMusicPath;
    QString statusMsg;
    bool needToFixVa;
    bool jobAbortRequested;
    bool transcoding;
    Encoders::Encoder encoder;
};

#endif
