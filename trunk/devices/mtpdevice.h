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
#include <libmtp.h>

class MusicLibraryItemRoot;
class QThread;

class MtpConnection : public QObject
{
    Q_OBJECT
public:
    MtpConnection();
    virtual ~MtpConnection();

    bool isConnected() const { return 0!=device; }
    MusicLibraryItemRoot * takeLibrary();
    uint64_t capacity() const { return size; }
    uint64_t usedSpace() const { return used; }

public Q_SLOTS:
    void connectToDevice();
    void disconnectFromDevice();
    void updateLibrary();
    void putSong(const Song &song);
    void getSong(const Song &song, const QString &dest);
    void delSong(const Song &song);

Q_SIGNALS:
    void statusMessage(const QString &message);
    void putSongStatus(bool, int, const QString &);
    void getSongStatus(bool);
    void delSongStatus(bool);
    void libraryUpdated();

private:
    void parseFolder(LIBMTP_folder_t *folder);
    uint32_t getMusicFolderId();
    uint32_t getFolderId(const char *name, LIBMTP_folder_t *f);
    void destroyData();

private:
    LIBMTP_mtpdevice_t *device;
    LIBMTP_folder_t *folders;
    LIBMTP_track_t *tracks;
    QMap<int, QString> folderMap;
    QMap<int, LIBMTP_track_t *> trackMap;
    MusicLibraryItemRoot *library;
    uint64_t size;
    uint64_t used;
    uint32_t musicFolderId;
};

class MtpDevice : public Device
{
    Q_OBJECT

public:
    MtpDevice(DevicesModel *m, Solid::Device &dev);
    virtual ~MtpDevice();

    bool isConnected() const;
    bool isRefreshing() const { return false; }// return 0!=scanner; }
    QString path() const { return QString(); } // audioFolder; }
    void addSong(const Song &s, bool overwrite);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite);
    void removeSong(const Song &s);
    void cleanDir(const QString &dir);
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();

Q_SIGNALS:
    // These are for talking to connection thread...
    void updateLibrary();
    void putSong(const Song &song);
    void getSong(const Song &song, const QString &dest);
    void delSong(const Song &song);

private Q_SLOTS:
    void libraryUpdated();
    void rescan();
//     void saveProperties(const QString &newPath, const Device::NameOptions &opts);
    void putSongStatus(bool ok, int id, const QString &file);
    void getSongStatus(bool ok);
    void delSongStatus(bool ok);

private:
    Solid::PortableMediaPlayer *pmp;
    QThread *thread;
    MtpConnection *connection;
    Song currentSong;
    bool mtpUpdating;
};

#endif
