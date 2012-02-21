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
#include "covers.h"
#include "mpdparseutils.h"
#include "encoders.h"
#include "transcodingjob.h"
#include "actiondialog.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <KDE/KUrl>
#include <KDE/KDiskFreeSpaceInfo>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KIO/FileCopyJob>
#include <KDE/KIO/Job>

static const QLatin1String constCantataCacheFile("/.cache.xml");

MusicScanner::MusicScanner(const QString &f)
    : QThread(0)
    , folder(f)
    , library(0)
    , stopRequested(false)
{
}

MusicScanner::~MusicScanner()
{
    delete library;
}

void MusicScanner::run()
{
    library = new MusicLibraryItemRoot;
    scanFolder(folder, 0);
}

void MusicScanner::stop()
{
    stopRequested=true;
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
                Song song=Tags::read(info.absoluteFilePath());

                if (song.isEmpty()) {
                    continue;
                }

                song.fillEmptyFields();
                song.size=info.size();
                if (!artistItem || song.albumArtist()!=artistItem->data()) {
                    artistItem = library->artist(song);
                }
                if (!albumItem || albumItem->parent()!=artistItem || song.album!=albumItem->data()) {
                    albumItem = artistItem->album(song);
                }
                MusicLibraryItemSong *songItem = new MusicLibraryItemSong(song, albumItem);

                albumItem->append(songItem);
                if (song.genre.isEmpty()) {
                    #ifdef ENABLE_KDE_SUPPORT
                    song.genre=i18n("Unknown");
                    #else
                    song.genre=QObject::tr("Unknown");
                    #endif
                }
                albumItem->addGenre(song.genre);
                artistItem->addGenre(song.genre);
                songItem->addGenre(song.genre);
                library->addGenre(song.genre);
            }
        }
    }
}

FsDevice::FsDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev)
    , scanner(0)
{
}

FsDevice::FsDevice(DevicesModel *m, const QString &name)
    : Device(m, name)
    , scanner(0)
{
}

FsDevice::~FsDevice() {
    stopScanner();
}

void FsDevice::rescan()
{
    if (isIdle()) {
        removeCache();
        startScanner();
    }
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

    KUrl dest(destFile);
    QDir dir(dest.directory());
    if(!dir.exists() && !Utils::createDir(dir.absolutePath(), QString())) {
        emit actionStatus(DirCreationFaild);
    }

    currentSong=s;
    if (encoder.codec.isEmpty() || (opts.transcoderWhenDifferent && !encoder.isDifferent(s.file))) {
        transcoding=false;
        KIO::FileCopyJob *job=KIO::file_copy(KUrl(s.file), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
        connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *)));
        connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(percent(KJob *, unsigned long)));
    } else {
        transcoding=true;
        TranscodingJob *job=new TranscodingJob(encoder.params(opts.transcoderValue, s.file, destFile));
        connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *)));
        connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(percent(KJob *, unsigned long)));
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

    QString source=s.file; // Device files have full path!!!

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
    KUrl dest(currentBaseDir+currentMusicPath);
    QDir dir(dest.directory());
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    KIO::FileCopyJob *job=KIO::file_copy(KUrl(source), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
    connect(job, SIGNAL(result(KJob *)), SLOT(copySongToResult(KJob *)));
    connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(percent(KJob *, unsigned long)));
}

void FsDevice::removeSong(const Song &s)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    if (!QFile::exists(s.file)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }

    currentSong=s;
    KIO::SimpleJob *job=KIO::file_delete(KUrl(s.file), KIO::HideProgressInfo);
    connect(job, SIGNAL(result(KJob *)), SLOT(removeSongResult(KJob *)));
}

void FsDevice::percent(KJob *job, unsigned long percent)
{
    if (jobAbortRequested && 100!=percent) {
        job->kill(KJob::EmitResult);
        return;
    }
    emit progress(percent);
}

void FsDevice::addSongResult(KJob *job)
{
    QString destFile=audioFolder+opts.createFilename(currentSong);
    if (transcoding) {
        destFile=encoder.changeExtension(destFile);
    }
    if (jobAbortRequested) {
        if (0!=job->percent() && 100!=job->percent() && QFile::exists(destFile)) {
            QFile::remove(destFile);
        }
        return;
    }
    if (job->error()) {
        emit actionStatus(transcoding ? TranscodeFailed : Failed);
    } else {
        QString sourceDir=MPDParseUtils::getDir(currentSong.file);

        currentSong.file=destFile;
        if (Device::constNoCover!=coverFileName) {
            Covers::copyCover(currentSong, sourceDir, MPDParseUtils::getDir(currentSong.file), coverFileName);
        }

        if (needToFixVa) {
            Device::fixVariousArtists(destFile, currentSong, true);
        }
        addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void FsDevice::copySongToResult(KJob *job)
{
    if (jobAbortRequested) {
        if (0!=job->percent() && 100!=job->percent() && QFile::exists(currentBaseDir+currentMusicPath)) {
            QFile::remove(currentBaseDir+currentMusicPath);
        }
        return;
    }
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        QString sourceDir=MPDParseUtils::getDir(currentSong.file);
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        Covers::copyCover(currentSong, sourceDir, currentBaseDir+MPDParseUtils::getDir(currentMusicPath), QString());
        if (needToFixVa) {
            Device::fixVariousArtists(currentBaseDir+currentSong.file, currentSong, false);
        }
        Utils::setFilePerms(currentBaseDir+currentSong.file);
        MusicLibraryModel::self()->addSongToList(currentSong);
        DirViewModel::self()->addFileToList(currentSong.file);
        emit actionStatus(Ok);
    }
}

void FsDevice::removeSongResult(KJob *job)
{
    if (jobAbortRequested) {
        return;
    }
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
}

void FsDevice::cacheRead()
{
    setStatusMessage(QString());
    emit updating(udi(), false);
}

void FsDevice::startScanner()
{
    stopScanner();
    scanner=new MusicScanner(audioFolder);
    connect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
    scanner->start();
    setStatusMessage(i18n("Updating..."));
    emit updating(udi(), true);
}

void FsDevice::stopScanner()
{
    // Scan for music in a separate thread...
    if (scanner) {
        disconnect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
        scanner->deleteLater();
        scanner->stop();
        scanner=0;
        setStatusMessage(QString());
        emit updating(udi(), false);
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
            update->toXML(cacheFileName(), audioFolder);
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
    return audioFolder+constCantataCacheFile;
}

void FsDevice::saveCache()
{
    if (opts.useCache) {
        toXML(cacheFileName(), audioFolder);
    }
}

void FsDevice::removeCache()
{
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }
}
