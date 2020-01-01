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

#ifndef DEVICE_H
#define DEVICE_H

#include "models/musiclibraryitemroot.h"
#include "mpd-interface/song.h"
#include "gui/covers.h"
#include "config.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "deviceoptions.h"
#include "solid-lite/device.h"
#endif

class QWidget;
class QImage;
class QTemporaryFile;
class MusicLibraryModel;

// MOC requires the QObject class to be first. But due to models storing void pointers, and
// needing to cast these - the model prefers MusicLibraryItemRoot to be first!
#ifdef Q_MOC_RUN
class Device : public QObject, public MusicLibraryItemRoot
#else
class Device : public MusicLibraryItemRoot, public QObject
#endif
{
    Q_OBJECT

public:
    #ifdef ENABLE_DEVICES_SUPPORT
    static const QLatin1String constNoCover;
    static const QLatin1String constEmbedCover;
    static Device * create(MusicLibraryModel *m, const QString &id);
    static bool fixVariousArtists(const QString &file, Song &song, bool applyFix);
    static void embedCover(const QString &file, Song &song, unsigned int coverMaxSize);
    static QTemporaryFile * copySongToTemp(Song &song);
    static bool isLossless(const QString &file);
    #endif
    static void moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile);
    static void cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level=0);

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
        FailedToCreateTempFile,
        ReadFailed,
        WriteFailed,
        NoSpace,
        FailedToUpdateTags,
        Cancelled,
        FailedToLockDevice,

        // These are for online services...
        DownloadFailed
    };

    enum DevType {
        Ums,
        Mtp,
        RemoteFs,
        AudioCd
    };

    #ifndef ENABLE_DEVICES_SUPPORT
    Device(MusicLibraryModel *m, const QString &name, const QString &id)
        : MusicLibraryItemRoot(name)
        , update(0)
        , needToFixVa(false)
        , jobAbortRequested(false)
        , transcoding(false) {
        Q_UNUSED(m)
        Q_UNUSED(id)
    }
    #else
    Device(MusicLibraryModel *m, Solid::Device &dev, bool albumArtistSupport=true, bool flat=false);
    Device(MusicLibraryModel *m, const QString &name, const QString &id);
    #endif

    ~Device() override { }

    QIcon icon() const override { return icn; }
    Song fixPath(const Song &orig, bool fullPath) const override;
    virtual QString coverFile() const { return QString(); }
    virtual bool isConnected() const=0;
    virtual void rescan(bool full=true)=0;
    virtual void stop()=0;
    virtual void connectionStateChanged() { }
    virtual void toggle() { }
    virtual bool isRefreshing() const=0;
    bool isIdle() const { return isConnected() && !isRefreshing(); }
    virtual void configure(QWidget *) { }
    virtual QString path() const =0;
    virtual void addSong(const Song &s, bool overwrite, bool copyCover)=0;
    virtual void copySongTo(const Song &s, const QString &musicPath, bool overwrite, bool copyCover)=0;
    virtual void removeSong(const Song &s)=0;
    virtual void cleanDirs(const QSet<QString> &dirs)=0;
    virtual Covers::Image requestCover(const Song &) { return Covers::Image(); }
    virtual double usedCapacity()=0;
    virtual QString capacityString()=0;
    virtual qint64 freeSpace()=0;
    virtual DevType devType() const=0;
    virtual void removeCache() { }
    bool isDevice() const override { return true; }

    #ifdef ENABLE_DEVICES_SUPPORT
    virtual void saveCache();
    const QString & id() const override { return deviceId; }
    void applyUpdate();
    bool haveUpdate() const { return nullptr!=update; }
    int newRows() const { return update ? update->childCount() : 0; }
    const DeviceOptions & options() const { return opts; }
    void setOptions(const DeviceOptions &o) { opts=o; saveOptions(); }
    virtual void saveOptions()=0;
    const QString & statusMessage() const { return statusMsg; }
    bool isConfigured() { return configured; }
    virtual void abortJob() { jobAbortRequested=true; }
    bool abortRequested() const { return jobAbortRequested; }
    bool canPlaySongs() const override { return false; }
    virtual bool supportsDisconnect() const { return false; }
    virtual bool isStdFs() const { return false; }
    virtual QString subText() { return QString(); }
    QModelIndex index() const override;
    void updateStatus();

public Q_SLOTS:
    void setStatusMessage(const QString &message);
    void songCount(int c);
    void updatePercentage(int pc);
    #endif

Q_SIGNALS:
    void connected(const QString &id);
    void disconnected(const QString &id);
    void updating(const QString &id, bool s);
    void actionStatus(int status, bool copiedCover=false);
    void progress(int pc);
    void error(const QString &);
    void cover(const Song &song, const QImage &img);
    void cacheSaved();
    void configurationChanged();
    void play(const QList<Song> &songs);
    void updatedDetails(const QList<Song> &songs);
    void renamed();

protected:
    #ifdef ENABLE_DEVICES_SUPPORT
    DeviceOptions opts;
    bool configured;
    Solid::Device solidDev;
    QString deviceId;
    Song currentSong;
    #endif
    MusicLibraryItemRoot *update;
    QString currentDestFile;
    QString statusMsg;
    bool needToFixVa;
    bool jobAbortRequested;
    bool transcoding;
    QIcon icn;
};

#endif
