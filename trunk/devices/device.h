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

#ifndef DEVICE_H
#define DEVICE_H

#ifdef Q_OS_WIN
namespace Device
{
    extern void moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile);
    extern void cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level=0);
}
#else

#include "musiclibraryitemroot.h"
#include "song.h"
#include "encoders.h"
#include "deviceoptions.h"
#include <QtCore/QObject>
#ifdef ENABLE_KDE_SUPPORT
#include <solid/device.h>
#else
#include "solid-lite/device.h"
#endif

class QWidget;
class QImage;
class QTemporaryFile;
class DevicesModel;

class Device : public MusicLibraryItemRoot
{
    Q_OBJECT

public:
    static Device * create(DevicesModel *m, const QString &udi);
    static bool fixVariousArtists(const QString &file, Song &song, bool applyFix);
    static void embedCover(const QString &file, Song &song, unsigned int coverMaxSize);
    static QTemporaryFile * copySongToTemp(Song &song);
    static void moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile);
    static void cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level=0);

    static const QLatin1String constNoCover;
    static const QLatin1String constEmbedCover;

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
        RemoteFs
    };

    Device(DevicesModel *m, Solid::Device &dev, bool albumArtistSupport=true)
        : MusicLibraryItemRoot(dev.vendor()+QChar(' ')+dev.product(), albumArtistSupport)
        , model(m)
        , configured(false)
        , solidDev(dev)
        , deviceId(dev.udi())
        , update(0)
        , needToFixVa(false)
        , jobAbortRequested(false)
        , transcoding(false) {
    }
    Device(DevicesModel *m, const QString &name, const QString &id)
        : MusicLibraryItemRoot(name)
        , model(m)
        , configured(false)
        , deviceId(id)
        , update(0)
        , needToFixVa(false)
        , jobAbortRequested(false)
        , transcoding(false) {
    }
    virtual ~Device() {
    }

    virtual QString icon() const {
        return solidDev.isValid() ? solidDev.icon() : QLatin1String("folder");
    }
    virtual QString coverFile() const {
        return QString();
    }

    virtual bool isConnected() const=0;
    virtual void rescan(bool full=true)=0;
    virtual void connectionStateChanged() { }
    virtual void toggle() { }
    virtual bool isRefreshing() const=0;
    bool isIdle() const { return isConnected() && !isRefreshing(); }
    virtual void configure(QWidget *) { }
    virtual QString path() const =0;
    virtual void addSong(const Song &s, bool overwrite, bool copyCover)=0;
    virtual void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover)=0;
    virtual void removeSong(const Song &s)=0;
    virtual void cleanDirs(const QSet<QString> &dirs)=0;
    virtual void requestCover(const Song &) { }
    virtual double usedCapacity()=0;
    virtual QString capacityString()=0;
    virtual qint64 freeSpace()=0;
    virtual DevType devType() const=0;
    virtual void saveCache() {
    }
    virtual void removeCache() {
    }

    const QString & udi() const {
        return deviceId;
    }
    void applyUpdate();
    bool haveUpdate() const {
        return 0!=update;
    }
    int newRows() const {
        return update ? update->childCount() : 0;
    }
    const DeviceOptions & options() const {
        return opts;
    }
    void setOptions(const DeviceOptions &o) {
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
    virtual bool supportsDisconnect() const {
        return false;
    }
    virtual bool isStdFs() const {
        return false;
    }

public Q_SLOTS:
    void setStatusMessage(const QString &message);
    void songCount(int c);

public:
    bool updateSong(const Song &orig, const Song &edit);
    void addSongToList(const Song &s);
    void removeSongFromList(const Song &s);

Q_SIGNALS:
    void connected(const QString &udi);
    void disconnected(const QString &udi);
    void updating(const QString &udi, bool s);
    void actionStatus(int status, bool copiedCover=false);
    void progress(int pc);
    void error(const QString &);
    void cover(const Song &song, const QImage &img);

protected:
    DevicesModel *model;
    DeviceOptions opts;
    bool configured;
    Solid::Device solidDev;
    QString deviceId;
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

#endif // Q_OS_WIN

#endif
