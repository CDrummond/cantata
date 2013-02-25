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

#ifndef MTPDEVICE_H
#define MTPDEVICE_H

#include "fsdevice.h"
#include "song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <solid/portablemediaplayer.h>
#else
#include "solid-lite/portablemediaplayer.h"
#endif
#include <libmtp.h>

class MusicLibraryItemRoot;
class QThread;
class MtpDevice;
class QTemporaryFile;

class MtpConnection : public QObject
{
    Q_OBJECT
public:
    struct Folder {
        Folder(const QString &pth=QString(), uint32_t i=0, uint32_t p=0, uint32_t s=0)
            : path(pth)
            , id(i)
            , parentId(p)
            , storageId(s) {
        }
        QString path;
        uint32_t id;
        uint32_t parentId;
        uint32_t storageId;
    };

    struct Storage : public DeviceStorage {
        Storage() : id(0), musicFolderId(0) { }
        uint32_t id;
        uint32_t musicFolderId;
        //uint32_t albumsFolderId;
        QString musicPath;
    };

    struct FolderId {
        FolderId(uint32_t i, uint32_t s) : id(i), storage(s) { }
        uint32_t id;
        uint32_t storage;
    };

    MtpConnection(MtpDevice *p);
    virtual ~MtpConnection();

    bool isConnected() const { return 0!=device; }
    MusicLibraryItemRoot * takeLibrary();
    uint64_t capacity() const { return size; }
    uint64_t usedSpace() const { return used; }
    QList<DeviceStorage> getStorageList() const;
    void emitProgress(int percent);
    void trackListProgress(uint64_t count);
    bool abortRequested() const;

public Q_SLOTS:
    void connectToDevice();
    void disconnectFromDevice(bool showStatus=true);
    void updateLibrary();
    void putSong(const Song &song, bool fixVa, const DeviceOptions &opts);
    void getSong(const Song &song, const QString &dest, bool fixVa, bool copyCover);
    void delSong(const Song &song);
    void cleanDirs(const QSet<QString> &dirs);
    void getCover(const Song &song);

Q_SIGNALS:
    void statusMessage(const QString &message);
    void putSongStatus(bool, int, const QString &, bool);
    void getSongStatus(bool ok, bool copiedCover);
    void delSongStatus(bool);
    void cleanDirsStatus(bool ok);
    void libraryUpdated();
    void progress(int);
    void deviceDetails(const QString &serialNumber);
    void songCount(int);
    void cover(const Song &s, const QImage &img);

private:
    void updateFolders();
    void updateAlbums();
    void updateStorage();
    Storage & getStorage(const QString &volumeIdentifier);
    uint32_t createFolder(const QString &name, const QString &fullPath, uint32_t parentId, uint32_t storageId);
    uint32_t getFolder(const QString &path, uint32_t storageId);
    QString getPath(uint32_t folderId);
    uint32_t checkFolderStructure(const QStringList &dirs, Storage &store);
    void parseFolder(LIBMTP_folder_t *folder);
    void setMusicFolder(Storage &store);
    //uint32_t getAlbumsFolderId();
    uint32_t getFolderId(const char *name, LIBMTP_folder_t *f, uint32_t storageId);
    LIBMTP_album_t * getAlbum(const Song &song);
    QImage getCover(LIBMTP_album_t *album);
    void destroyData();

private:
    LIBMTP_mtpdevice_t *device;
    LIBMTP_folder_t *folders;
    LIBMTP_album_t *albums;
    LIBMTP_track_t *tracks;
    QMap<int, Folder> folderMap;
    QMap<int, LIBMTP_track_t *> trackMap;
    //QSet<uint16_t> supportedTypes;
    QSet<LIBMTP_album_t *> albumsWithCovers;
    MusicLibraryItemRoot *library;
    uint32_t defaultMusicFolder;
    QList<Storage> storage;
    uint64_t size;
    uint64_t used;
    MtpDevice *dev;
    int lastUpdate;
};

class MtpDevice : public Device
{
    Q_OBJECT

public:
    MtpDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~MtpDevice();

    bool isConnected() const;
    bool isRefreshing() const { return mtpUpdating; }
    void stop();
    void configure(QWidget *parent);
    QString path() const { return QString(); } // audioFolder; }
    void addSong(const Song &s, bool overwrite, bool copyCover);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover);
    void removeSong(const Song &s);
    void cleanDirs(const QSet<QString> &dirs);
    void requestCover(const Song &song);
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();
    DevType devType() const { return Mtp; }
    void saveOptions();

Q_SIGNALS:
    // These are for talking to connection thread...
    void updateLibrary();
    void putSong(const Song &song, bool fixVa, const DeviceOptions &opts);
    void getSong(const Song &song, const QString &dest, bool fixVa, bool copyCover);
    void delSong(const Song &song);
    void cleanMusicDirs(const QSet<QString> &dirs);
    void getCover(const Song &s);
    void cover(const Song &s, const QImage &img);

private Q_SLOTS:
    void deviceDetails(const QString &s);
    void libraryUpdated();
    void rescan(bool full=true);
    void putSongStatus(bool ok, int id, const QString &file, bool fixedVa);
    void transcodeSongResult(int status);
    void transcodePercent(int percent);
    void emitProgress(int);
    void getSongStatus(bool ok, bool copiedCover);
    void delSongStatus(bool ok);
    void cleanDirsStatus(bool ok);
    void saveProperties(const QString &newPath, const DeviceOptions &opts);
    void saveProperties();

private:
    int getSongId(const Song &s);
    void deleteTemp();

private:
    Solid::PortableMediaPlayer *pmp;
    QThread *thread;
    MtpConnection *connection;
    QTemporaryFile *tempFile;
    Song currentSong;
    bool mtpUpdating;
    QString serial;
    unsigned int busNum;
    unsigned int devNum;
    friend class MtpConnection;
};

#endif
