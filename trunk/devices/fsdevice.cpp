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

#include "umsdevice.h"
#include "tags.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "dirviewmodel.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "utils.h"
#include "mpdparseutils.h"
#include "encoders.h"
#include "transcodingjob.h"
#include "actiondialog.h"
#include "localize.h"
#include "covers.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <time.h>

static const QLatin1String constCantataCacheFile("/.cache.xml");

MusicScanner::MusicScanner(const QString &f)
    : QThread(0)
    , library(0)
    , stopRequested(false)
    , count(0)
    , lastUpdate(0)
{
    folder=Utils::fixPath(QDir(f).absolutePath());
}

MusicScanner::~MusicScanner()
{
    delete library;
}

void MusicScanner::run()
{
    count=0;
    lastUpdate=0;
    library = new MusicLibraryItemRoot;
    scanFolder(folder, 0);
    if (MPDParseUtils::groupSingle()) {
        library->groupSingleTracks();
    }
    if (MPDParseUtils::groupMultiple()) {
        library->groupMultipleArtists();
    }
}

void MusicScanner::stop()
{
    stopRequested=true;
    Utils::stopThread(this);
}

MusicLibraryItemRoot * MusicScanner::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

void MusicScanner::scanFolder(const QString &f, int level)
{
    if (stopRequested) {
        return;
    }
    if (level<4) {
        QDir d(f);
        QFileInfoList entries=d.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Dirs|QDir::NoDotAndDotDot);
        MusicLibraryItemArtist *artistItem = 0;
        MusicLibraryItemAlbum *albumItem = 0;
        foreach (const QFileInfo &info, entries) {
            if (stopRequested) {
                return;
            }
            if (info.isDir()) {
                scanFolder(info.absoluteFilePath(), level+1);
            } else if(info.isReadable()) {
                Song song;
                QString fname=info.absoluteFilePath().mid(folder.length());
                QSet<Song>::iterator it=existing.find(song);
                if (existing.end()==it) {
                    song=Tags::read(info.absoluteFilePath());
                } else {
                    song=*it;
                    existing.erase(it);
                }
                song.file=fname;

                if (song.isEmpty()) {
                    continue;
                }
                count++;
                int t=time(NULL);
                if ((t-lastUpdate)>=2 || 0==(count%5)) {
                    lastUpdate=t;
                    emit songCount(count);
                }

                song.fillEmptyFields();
                song.size=info.size();
                if (!artistItem || song.albumArtist()!=artistItem->data()) {
                    artistItem = library->artist(song);
                }
                if (!albumItem || albumItem->parentItem()!=artistItem || song.album!=albumItem->data()) {
                    albumItem = artistItem->album(song);
                }
                MusicLibraryItemSong *songItem = new MusicLibraryItemSong(song, albumItem);
                albumItem->append(songItem);
                albumItem->addGenre(song.genre);
                artistItem->addGenre(song.genre);
                library->addGenre(song.genre);
            }
        }
    }
}

FsDevice::FsDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev)
    , scanned(false)
    , scanner(0)
{
}

FsDevice::FsDevice(DevicesModel *m, const QString &name, const QString &id)
    : Device(m, name, id)
    , scanned(false)
    , scanner(0)
{
}

FsDevice::~FsDevice() {
    stopScanner(false);
}

void FsDevice::rescan(bool full)
{
    spaceInfo.setDirty();
    // If this is the first scan (scanned=false) and we are set to use cache, attempt to load that before scanning
    if (isIdle() && (scanned || !opts.useCache || !readCache())) {
        scanned=true;
        if (full) {
            clear();
        }
        removeCache();
        startScanner(full);
    }
}

bool FsDevice::readCache()
{
    if (opts.useCache) {
        MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
        if (root->fromXML(cacheFileName(), QDateTime(), audioFolder)) {
            update=root;
            scanned=true;
            QTimer::singleShot(0, this, SLOT(cacheRead()));
            return true;
        }
        delete root;
    }

    return false;
}

void FsDevice::addSong(const Song &s, bool overwrite)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, true);
        }
        if (songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    if (!QFile::exists(s.file)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }

    QString destFile=audioFolder+opts.createFilename(s);

    if (!opts.transcoderCodec.isEmpty()) {
        encoder=Encoders::getEncoder(opts.transcoderCodec);
        if (encoder.codec.isEmpty()) {
            emit actionStatus(CodecNotAvailable);
            return;
        }
        destFile=encoder.changeExtension(destFile);
    }

    if (!overwrite && QFile::exists(destFile)) {
        emit actionStatus(FileExists);
        return;
    }

    QDir dir(Utils::getDir(destFile));
    if(!dir.exists() && !Utils::createDir(dir.absolutePath(), QString())) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    if (encoder.codec.isEmpty() || (opts.transcoderWhenDifferent && !encoder.isDifferent(s.file))) {
        transcoding=false;
        CopyJob *job=new CopyJob(s.file, destFile, coverOpts, (needToFixVa ? CopyJob::OptsApplyVaFix : CopyJob::OptsNone)|
                                                              (Device::RemoteFs==devType() ? CopyJob::OptsFixLocal : CopyJob::OptsNone),
                                 currentSong);
        connect(job, SIGNAL(result(int)), SLOT(addSongResult(int)));
        connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
        job->start();
    } else {
        transcoding=true;
        TranscodingJob *job=new TranscodingJob(encoder, opts.transcoderValue, s.file, destFile, coverOpts,
                                               (needToFixVa ? CopyJob::OptsApplyVaFix : CopyJob::OptsNone)|
                                               (Device::RemoteFs==devType() ? CopyJob::OptsFixLocal : CopyJob::OptsNone));
        connect(job, SIGNAL(result(int)), SLOT(addSongResult(int)));
        connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
        job->start();
    }
}

void FsDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, false);
        }
        if (MusicLibraryModel::self()->songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    QString source=audioFolder+s.file;

    if (!QFile::exists(source)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }

    if (!overwrite && QFile::exists(baseDir+musicPath)) {
        emit actionStatus(FileExists);
        return;
    }

    currentBaseDir=baseDir;
    currentMusicPath=musicPath;
    QString dest(currentBaseDir+currentMusicPath);
    QDir dir(Utils::getDir(dest));
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    CopyJob *job=new CopyJob(source, dest, CoverOptions(), needToFixVa ? CopyJob::OptsUnApplyVaFix : CopyJob::OptsNone, currentSong);
    connect(job, SIGNAL(result(int)), SLOT(copySongToResult(int)));
    connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
    job->start();
}

void FsDevice::removeSong(const Song &s)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    if (!QFile::exists(audioFolder+s.file)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }

    currentSong=s;
    DeleteJob *job=new DeleteJob(audioFolder+s.file);
    connect(job, SIGNAL(result(int)), SLOT(removeSongResult(int)));
    job->start();
}

void FsDevice::cleanDirs(const QSet<QString> &dirs)
{
    CleanJob *job=new CleanJob(dirs, audioFolder, coverOpts.name);
    connect(job, SIGNAL(result(int)), SLOT(cleanDirsResult(int)));
    connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
    job->start();
}

void FsDevice::requestCover(const Song &s)
{
    QString songFile=audioFolder+s.file;
    QString dirName=Utils::getDir(songFile);

    if (QFile::exists(dirName+coverOpts.name)) {
        QImage img(dirName+coverOpts.name);
        if (!img.isNull()) {
            emit cover(s, img);
            return;
        }
    }

    QStringList files=QDir(dirName).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
    foreach (const QString &fileName, files) {
        QImage img(dirName+fileName);

        if (!img.isNull()) {
            emit cover(s, img);
            return;
        }
    }
}

void FsDevice::percent(int pc)
{
    if (jobAbortRequested && 100!=pc) {
        FileJob *job=qobject_cast<FileJob *>(sender());
        if (job) {
            job->stop();
        }
        return;
    }
    emit progress(pc);
}

void FsDevice::addSongResult(int status)
{
    spaceInfo.setDirty();
    QString destFileName=opts.createFilename(currentSong);
    if (transcoding) {
        destFileName=encoder.changeExtension(destFileName);
    }
    if (jobAbortRequested) {
        FileJob *job=qobject_cast<FileJob *>(sender());
        if (job && job->wasStarted() && QFile::exists(audioFolder+destFileName)) {
            QFile::remove(audioFolder+destFileName);
        }
        return;
    }
    if (FileJob::StatusOk!=status) {
        emit actionStatus(transcoding ? TranscodeFailed : Failed);
    } else {
        currentSong.file=destFileName;
        if (needToFixVa) {
            currentSong.fixVariousArtists();
        }
        addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void FsDevice::copySongToResult(int status)
{
    spaceInfo.setDirty();
    if (jobAbortRequested) {
        FileJob *job=qobject_cast<FileJob *>(sender());
        if (job && job->wasStarted() && QFile::exists(currentBaseDir+currentMusicPath)) {
            QFile::remove(currentBaseDir+currentMusicPath);
        }
        return;
    }
    if (FileJob::StatusOk!=status) {
        emit actionStatus(Failed);
    } else {
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        if (needToFixVa) {
            currentSong.revertVariousArtists();
        }
        Utils::setFilePerms(currentBaseDir+currentSong.file);
        MusicLibraryModel::self()->addSongToList(currentSong);
        DirViewModel::self()->addFileToList(currentSong.file);
        emit actionStatus(Ok);
    }
}

void FsDevice::removeSongResult(int status)
{
    spaceInfo.setDirty();
    if (jobAbortRequested) {
        return;
    }
    if (FileJob::StatusOk!=status) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
}

void FsDevice::cleanDirsResult(int status)
{
    spaceInfo.setDirty();
    if (jobAbortRequested) {
        return;
    }
    emit actionStatus(FileJob::StatusOk==status ? Ok : Failed);
}

void FsDevice::cacheRead()
{
    setStatusMessage(QString());
    emit updating(udi(), false);
}

void FsDevice::startScanner(bool fullScan)
{
    stopScanner();
    scanner=new MusicScanner(audioFolder);
    connect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
    connect(scanner, SIGNAL(songCount(int)), this, SLOT(songCount(int)));
    QSet<Song> existingSongs;
    if (!fullScan) {
        existingSongs=allSongs();
    }
    scanner->start(existingSongs);
    setStatusMessage(i18n("Updating..."));
    emit updating(udi(), true);
}

void FsDevice::stopScanner(bool showStatus)
{
    // Scan for music in a separate thread...
    if (scanner) {
        disconnect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
        disconnect(scanner, SIGNAL(songCount(int)), this, SLOT(songCount(int)));
        scanner->deleteLater();
        scanner->stop();
        scanner=0;
        if (showStatus) {
            setStatusMessage(QString());
            emit updating(udi(), false);
        }
    }
}

void FsDevice::clear() const
{
    if (childCount()) {
        FsDevice *that=const_cast<FsDevice *>(this);
        that->update=new MusicLibraryItemRoot();
        that->applyUpdate();
        that->scanned=false;
    }
}

void FsDevice::libraryUpdated()
{
    if (scanner) {
        if (update) {
            delete update;
        }
        update=scanner->takeLibrary();
        if (opts.useCache && update) {
            update->toXML(cacheFileName());
        }
        stopScanner();
    }
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

QString FsDevice::cacheFileName() const
{
    if (audioFolder.isEmpty()) {
        setAudioFolder();
    }
    return audioFolder+constCantataCacheFile;
}

void FsDevice::saveCache()
{
    if (opts.useCache) {
        toXML(cacheFileName());
    }
}

void FsDevice::removeCache()
{
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }
}
