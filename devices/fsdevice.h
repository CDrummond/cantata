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
#include <solid/storageaccess.h>
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
    void stop();
    bool wasStopped() const { return stopRequested; }
    MusicLibraryItemRoot * takeLibrary();

Q_SIGNALS:
    void songCount(int c);

private:
    void scanFolder(const QString &f, int level);

private:
    QString folder;
    MusicLibraryItemRoot *library;
    bool stopRequested;
    int count;
};

class FsDevice : public Device
{
    Q_OBJECT

public:
    FsDevice(DevicesModel *m, Solid::Device &dev);
    FsDevice(DevicesModel *m, const QString &name);
    virtual ~FsDevice();

    void rescan();
    bool isRefreshing() const { return 0!=scanner; }
    QString path() const { return audioFolder; }
    QString coverFile() const { return coverFileName; }
    void addSong(const Song &s, bool overwrite);
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite);
    void removeSong(const Song &s);
    void cleanDir(const QString &dir) { Utils::cleanDir(dir, audioFolder, coverFileName); }
    QString cacheFileName() const;
    void saveCache();
    void removeCache();

protected:
    void startScanner();
    void stopScanner();

protected Q_SLOTS:
    void cacheRead();
    void libraryUpdated();
    void percent(KJob *job, unsigned long percent);
    void addSongResult(KJob *job);
    void copySongToResult(KJob *job);
    void removeSongResult(KJob *job);

protected:
    MusicScanner *scanner;
    QString audioFolder;
    QString coverFileName;
};

#endif
