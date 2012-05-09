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

#ifndef REMOTEKIODEVICE_H
#define REMOTEKIODEVICE_H

#include "device.h"
#include "song.h"
#include "remotefsdevice.h"
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <KDE/KUrl>
#include <KDE/KIO/Job>

class KTemporaryFile;
class MusicLibraryItemArtist;
class MusicLibraryItemAlbum;

class KioScanner : public QObject
{
    Q_OBJECT

public:
    KioScanner(QObject *parent);
    ~KioScanner();

    void scan(const KUrl &url, const QSet<Song> &existingSongs);
    void stop(bool killJobs=true);

    MusicLibraryItemRoot * takeLibrary();

Q_SIGNALS:
    void status(const QString &errorMsg);
    void songCount(int);

private Q_SLOTS:
    void dirEntries(KIO::Job *job, const KIO::UDSEntryList &l);
    void dirResult(KJob *job);
    void fileData(KIO::Job *job, const QByteArray &data);
    void fileResult(KJob *job);
    void fileOpen(KIO::Job *job);

private:
    void listDir(const KUrl &u);
    void processNextEntry();
    void addSong(const Song &s);

private:
    struct Data {
        KUrl url;
        QByteArray data;
    };
    bool terminated;
    int count;
    KUrl top;
    QMap<KJob *, Data> jobData;
    QMap<KJob *, KUrl> dirData;
    KUrl::List files;
    KUrl::List dirs;
    QSet<KJob *> activeJobs;
    QMap<QString, KTemporaryFile *> tempFiles;
    QSet<Song> existing;
    MusicLibraryItemRoot *library;
    MusicLibraryItemArtist *artistItem;
    MusicLibraryItemAlbum *albumItem;
    QElapsedTimer timer;
};

class RemoteKioDevice : public Device
{
    Q_OBJECT

public:
    RemoteKioDevice(DevicesModel *m, const QString &cover, const Options &options, const RemoteFsDevice::Details &d);
    RemoteKioDevice(DevicesModel *m, const RemoteFsDevice::Details &d);
    virtual ~RemoteKioDevice();

    bool isConnected() const;
    bool isRefreshing() const { return false; }
    void configure(QWidget *parent);
    QString path() const { return QString(); }
    void addSong(const Song &s, bool overwrite);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite);
    void removeSong(const Song &s);
    void cleanDirs(const QSet<QString> &dirs);
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();
    DevType devType() const { return RemoteKio; }
    void saveOptions();
    QString icon() const {
        return QLatin1String("applications-internet");
    }
    QString udi() const { return RemoteFsDevice::createUdi(details.name); }

Q_SIGNALS:
    void udiChanged(const QString &from, const QString &to);

private:
    bool readCache();
    void startScanner(bool fullScan=true);
    void stopScanner(bool showStatus=true);

private Q_SLOTS:
    void addSongResult(KJob *job);
    void copySongToResult(KJob *job);
    void removeSongResult(KJob *job);
    void transcodeSongResult(KJob *job);
    void transcodePercent(KJob *job, unsigned long percent);
    void jobProgress(KJob *job, unsigned long percent);
    void libraryUpdated(const QString &errorMsg);
    void rescan(bool full=true);
    void saveProperties();
    void saveProperties(const QString &newCoverFileName, const Device::Options &newOpts, RemoteFsDevice::Details newDetails);
    void cacheRead();

private:
    KUrl cacheUrl();
    void saveCache();
    void removeCache();
    void deleteTemp();
    QString createTemp(const QString &ext);

private:
    bool scanned;
    KioScanner *scanner;
    RemoteFsDevice::Details details;
    KTemporaryFile *tempFile;
    Song currentSong;
    QString coverFileName;
    bool overWrite;

    friend class RemoteFsDevice;
};

#endif
