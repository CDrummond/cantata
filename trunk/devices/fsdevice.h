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

#ifndef FSDEVICE_H
#define FSDEVICE_H

#include "device.h"
#include "song.h"
#include "utils.h"
#include "freespaceinfo.h"
#include <QtCore/QThread>
#include <QtCore/QStringList>

class MusicLibraryItemRoot;
class KJob;

class MusicScanner : public QThread
{
    Q_OBJECT

public:
    MusicScanner(const QString &f);
    virtual ~MusicScanner();

    void run();
    void start(const QSet<Song> &existingSongs) {
        existing=existingSongs;
        QThread::start();
    }
    void stop();
    bool wasStopped() const { return stopRequested; }
    MusicLibraryItemRoot * takeLibrary();

Q_SIGNALS:
    void songCount(int c);

private:
    void scanFolder(const QString &f, int level);

private:
    QSet<Song> existing;
    QString folder;
    MusicLibraryItemRoot *library;
    bool stopRequested;
    int count;
    int lastUpdate;
};

class FsDevice : public Device
{
    Q_OBJECT

public:
    struct CoverOptions
    {
        CoverOptions(const QString &n=QString()) : name(n), maxSize(0) { }
        void checkSize() {
            if (0==maxSize || maxSize>=400) {
                maxSize=0;
            } else {
                maxSize=((int)(maxSize/100))*100;
            }
        }

        QString name;
        unsigned int maxSize;
    };

    FsDevice(DevicesModel *m, Solid::Device &dev);
    FsDevice(DevicesModel *m, const QString &name, const QString &id);
    virtual ~FsDevice();

    void rescan(bool full=true);
    bool isRefreshing() const { return 0!=scanner; }
    QString path() const { return audioFolder; }
    QString coverFile() const { return coverOpts.name; }
    void addSong(const Song &s, bool overwrite, bool copyCover);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover);
    void removeSong(const Song &s);
    void cleanDirs(const QSet<QString> &dirs);
    void requestCover(const Song &s);
    QString cacheFileName() const;
    virtual void setAudioFolder() const { }
    void saveCache();
    void removeCache();
    bool isStdFs() const { return true; }

protected:
    bool readCache();
    void startScanner(bool fullScan=true);
    void stopScanner(bool showStatus=true);
    void clear() const;

protected Q_SLOTS:
    void cacheRead();
    void libraryUpdated();
    void percent(int pc);
    void addSongResult(int status);
    void copySongToResult(int status);
    void removeSongResult(int status);
    void cleanDirsResult(int status);

protected:
    bool scanned;
    MusicScanner *scanner;
    mutable QString audioFolder;
    CoverOptions coverOpts;
    FreeSpaceInfo spaceInfo;
};

#endif
