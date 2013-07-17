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

#ifndef FSDEVICE_H
#define FSDEVICE_H

#include "device.h"
#include "song.h"
#include "utils.h"
#include "freespaceinfo.h"
#include "musiclibraryitemroot.h"
#include <QStringList>

class Thread;

struct FileOnlySong : public Song
{
    FileOnlySong(const Song &o) : Song(o) { }
    bool operator==(const FileOnlySong &o) const { return file==o.file; }
    bool operator<(const FileOnlySong &o) const { return file.compare(o.file)<0; }
};

inline uint qHash(const FileOnlySong &key)
{
    return qHash(key.file);
}

class MusicScanner : public QObject, public MusicLibraryProgressMonitor
{
    Q_OBJECT

public:
    MusicScanner();
    virtual ~MusicScanner();

    void stop();
    bool wasStopped() const { return stopRequested; }
    void readProgress(double pc);
    void writeProgress(double pc);

public Q_SLOTS:
    void scan(const QString &folder, const QString &cacheFile, bool readCache, const QSet<FileOnlySong> &existingSongs);
    void saveCache(const QString &cache, MusicLibraryItemRoot *lib);

Q_SIGNALS:
    void songCount(int c);
    void libraryUpdated(MusicLibraryItemRoot *);
    void cacheSaved();
    void readingCache(int pc);
    void savingCache(int pc);

private:
    void fixLibrary(MusicLibraryItemRoot *lib);
    void scanFolder(MusicLibraryItemRoot *library, const QString &topLevel, const QString &f, QSet<FileOnlySong> &existing, int level);

private:
    Thread *thread;
    bool stopRequested;
    int count;
    int lastUpdate;
    int lastCacheProg;
};

class FsDevice : public Device
{
    Q_OBJECT

public:
    enum State {
        Idle,
        Updating,
        SavingCache
    };

    static const QLatin1String constCantataCacheFile;
    static const QLatin1String constCantataSettingsFile;
    static const QLatin1String constMusicFilenameSchemeKey;
    static const QLatin1String constVfatSafeKey;
    static const QLatin1String constAsciiOnlyKey;
    static const QLatin1String constIgnoreTheKey;
    static const QLatin1String constReplaceSpacesKey;
    static const QLatin1String constCoverFileNameKey; // Cantata extension!
    static const QLatin1String constCoverMaxSizeKey; // Cantata extension!
    static const QLatin1String constVariousArtistsFixKey; // Cantata extension!
    static const QLatin1String constTranscoderKey; // Cantata extension!
    static const QLatin1String constUseCacheKey; // Cantata extension!
    static const QLatin1String constDefCoverFileName;
    static const QLatin1String constAutoScanKey; // Cantata extension!

    static bool readOpts(const QString &fileName, DeviceOptions &opts, bool readAll);
    static void writeOpts(const QString &fileName, const DeviceOptions &opts, bool writeAll);

    FsDevice(MusicModel *m, Solid::Device &dev);
    FsDevice(MusicModel *m, const QString &name, const QString &id);
    virtual ~FsDevice();

    void rescan(bool full=true);
    void stop();
    bool isRefreshing() const { return Idle!=state; }
    QString path() const { return audioFolder; }
    QString coverFile() const { return opts.coverName; }
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
    bool canPlaySongs() const { return true; }

Q_SIGNALS:
    // For talking to scanner...
    void scan(const QString &folder, const QString &cacheFile, bool readCache, const QSet<FileOnlySong> &existingSongs);
    void saveCache(const QString &cacheFile, MusicLibraryItemRoot *lib);

protected:
    void initScaner();
    void startScanner(bool fullScan=true);
    void stopScanner();
    void clear() const;

protected Q_SLOTS:
    void savedCache();
    void libraryUpdated(MusicLibraryItemRoot *lib);
    void percent(int pc);
    void addSongResult(int status);
    void copySongToResult(int status);
    void removeSongResult(int status);
    void cleanDirsResult(int status);
    void readingCache(int pc);
    void savingCache(int pc);

private:
    void cacheStatus(const QString &msg, int prog);

protected:
    State state;
    bool scanned;
    int cacheProgress;
    MusicScanner *scanner;
    mutable QString audioFolder;
    FreeSpaceInfo spaceInfo;
};

#endif
