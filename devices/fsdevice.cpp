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

const QLatin1String FsDevice::constCantataCacheFile("/.cache");
const QLatin1String FsDevice::constCantataSettingsFile("/.cantata");
const QLatin1String FsDevice::constMusicFilenameSchemeKey("music_filenamescheme");
const QLatin1String FsDevice::constVfatSafeKey("vfat_safe");
const QLatin1String FsDevice::constAsciiOnlyKey("ascii_only");
const QLatin1String FsDevice::constIgnoreTheKey("ignore_the");
const QLatin1String FsDevice::constReplaceSpacesKey("replace_spaces");
const QLatin1String FsDevice::constCoverFileNameKey("cover_filename"); // Cantata extension!
const QLatin1String FsDevice::constCoverMaxSizeKey("cover_maxsize"); // Cantata extension!
const QLatin1String FsDevice::constVariousArtistsFixKey("fix_various_artists"); // Cantata extension!
const QLatin1String FsDevice::constTranscoderKey("transcoder"); // Cantata extension!
const QLatin1String FsDevice::constUseCacheKey("use_cache"); // Cantata extension!
const QLatin1String FsDevice::constDefCoverFileName("cover.jpg");
const QLatin1String FsDevice::constAutoScanKey("auto_scan"); // Cantata extension!

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

bool FsDevice::readOpts(const QString &fileName, DeviceOptions &opts, bool readAll)
{
    QFile file(fileName);

    opts=DeviceOptions(constDefCoverFileName);
    if (file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(constCoverFileNameKey+"=")) {
                opts.coverName=line.section('=', 1, 1);
            } if (line.startsWith(constCoverMaxSizeKey+"=")) {
                opts.coverMaxSize=line.section('=', 1, 1).toUInt();
                opts.checkCoverSize();
            } else if(line.startsWith(constVariousArtistsFixKey+"=")) {
                opts.fixVariousArtists=QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constTranscoderKey+"="))  {
                QStringList parts=line.section('=', 1, 1).split(',');
                if (3==parts.size()) {
                    opts.transcoderCodec=parts.at(0);
                    opts.transcoderValue=parts.at(1).toInt();
                    opts.transcoderWhenDifferent=QLatin1String("true")==parts.at(2);
                }
            } else if (line.startsWith(constUseCacheKey+"=")) {
                opts.useCache=QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constAutoScanKey+"=")) {
                opts.autoScan=QLatin1String("true")==line.section('=', 1, 1);
            } else if (readAll) {
                // For UMS these are stored in .is_audio_player - for Amarok compatability!
                if (line.startsWith(constMusicFilenameSchemeKey+"=")) {
                    QString scheme = line.section('=', 1, 1);
                    //protect against empty setting.
                    if (!scheme.isEmpty() ) {
                        opts.scheme = scheme;
                    }
                } else if (line.startsWith(constVfatSafeKey+"="))  {
                    opts.vfatSafe = QLatin1String("true")==line.section('=', 1, 1);
                } else if (line.startsWith(constAsciiOnlyKey+"=")) {
                    opts.asciiOnly = QLatin1String("true")==line.section('=', 1, 1);
                } else if (line.startsWith(constIgnoreTheKey+"=")) {
                    opts.ignoreThe = QLatin1String("true")==line.section('=', 1, 1);
                } else if (line.startsWith(constReplaceSpacesKey+"="))  {
                    opts.replaceSpaces = QLatin1String("true")==line.section('=', 1, 1);
                }
            }
        }

        return true;
    }
    return false;
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

void FsDevice::writeOpts(const QString &fileName, const DeviceOptions &opts, bool writeAll)
{
    DeviceOptions def(constDefCoverFileName);
    // If we are just using the defaults, then mayas wel lremove the file!
    if ( (writeAll && opts==def) ||
         (!writeAll && opts.coverName==constDefCoverFileName && 0==opts.coverMaxSize && opts.fixVariousArtists!=def.fixVariousArtists &&
          opts.transcoderCodec.isEmpty() && opts.useCache==def.useCache && opts.autoScan!=def.autoScan)) {
        if (QFile::exists(fileName)) {
            QFile::remove(fileName);
        }
        return;
    }

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly|QIODevice::Text)) {

        QTextStream out(&file);
        if (writeAll) {
            if (opts.scheme!=def.scheme) {
                out << constMusicFilenameSchemeKey << '=' << opts.scheme << '\n';
            }
            if (opts.scheme!=def.scheme) {
                out << constVfatSafeKey << '=' << toString(opts.vfatSafe) << '\n';
            }
            if (opts.asciiOnly!=def.asciiOnly) {
                out << constAsciiOnlyKey << '=' << toString(opts.asciiOnly) << '\n';
            }
            if (opts.ignoreThe!=def.ignoreThe) {
                out << constIgnoreTheKey << '=' << toString(opts.ignoreThe) << '\n';
            }
            if (opts.replaceSpaces!=def.replaceSpaces) {
                out << constReplaceSpacesKey << '=' << toString(opts.replaceSpaces) << '\n';
            }
        }

        // NOTE: If any options are added/changed - take care of the "if ( (writeAll..." block above!!!
        if (opts.coverName!=constDefCoverFileName) {
            out << constCoverFileNameKey << '=' << opts.coverName << '\n';
        }
        if (0!=opts.coverMaxSize) {
            out << constCoverMaxSizeKey << '=' << opts.coverMaxSize << '\n';
        }
        if (opts.fixVariousArtists!=def.fixVariousArtists) {
            out << constVariousArtistsFixKey << '=' << toString(opts.fixVariousArtists) << '\n';
        }
        if (!opts.transcoderCodec.isEmpty()) {
            out << constTranscoderKey << '=' << opts.transcoderCodec << ',' << opts.transcoderValue << ',' << toString(opts.transcoderWhenDifferent) << '\n';
        }
        if (opts.useCache!=def.useCache) {
            out << constUseCacheKey << '=' << toString(opts.useCache) << '\n';
        }
        if (opts.autoScan!=def.autoScan) {
            out << constAutoScanKey << '=' << toString(opts.autoScan) << '\n';
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
        MusicLibraryModel::convertCache(cacheFileName());
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

void FsDevice::addSong(const Song &s, bool overwrite, bool copyCover)
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
        CopyJob *job=new CopyJob(s.file, destFile, copyCover ? opts : DeviceOptions(Device::constNoCover),
                                 (needToFixVa ? CopyJob::OptsApplyVaFix : CopyJob::OptsNone)|(Device::RemoteFs==devType() ? CopyJob::OptsFixLocal : CopyJob::OptsNone),
                                 currentSong);
        connect(job, SIGNAL(result(int)), SLOT(addSongResult(int)));
        connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
        job->start();
    } else {
        transcoding=true;
        TranscodingJob *job=new TranscodingJob(encoder, opts.transcoderValue, s.file, destFile, copyCover ? opts : DeviceOptions(Device::constNoCover),
                                               (needToFixVa ? CopyJob::OptsApplyVaFix : CopyJob::OptsNone)|
                                               (Device::RemoteFs==devType() ? CopyJob::OptsFixLocal : CopyJob::OptsNone));
        connect(job, SIGNAL(result(int)), SLOT(addSongResult(int)));
        connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
        job->start();
    }
}

void FsDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover)
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
    CopyJob *job=new CopyJob(source, dest, copyCover ? opts : DeviceOptions(Device::constNoCover),
                             needToFixVa ? CopyJob::OptsUnApplyVaFix : CopyJob::OptsNone, currentSong);
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
    CleanJob *job=new CleanJob(dirs, audioFolder, opts.coverName);
    connect(job, SIGNAL(result(int)), SLOT(cleanDirsResult(int)));
    connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
    job->start();
}

void FsDevice::requestCover(const Song &s)
{
    QString songFile=audioFolder+s.file;
    QString dirName=Utils::getDir(songFile);

    if (QFile::exists(dirName+opts.coverName)) {
        QImage img(dirName+opts.coverName);
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
    CopyJob *job=qobject_cast<CopyJob *>(sender());
    FileJob::finished(job);
    spaceInfo.setDirty();
    QString destFileName=opts.createFilename(currentSong);
    if (transcoding) {
        destFileName=encoder.changeExtension(destFileName);
    }
    if (jobAbortRequested) {
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
        emit actionStatus(Ok, job && job->coverCopied());
    }
}

void FsDevice::copySongToResult(int status)
{
    CopyJob *job=qobject_cast<CopyJob *>(sender());
    FileJob::finished(job);
    spaceInfo.setDirty();
    if (jobAbortRequested) {
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
        emit actionStatus(Ok, job && job->coverCopied());
    }
}

void FsDevice::removeSongResult(int status)
{
    FileJob::finished(sender());
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
    FileJob::finished(sender());
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

QString FsDevice::cacheFileName() const
{
    if (audioFolder.isEmpty()) {
        setAudioFolder();
    }
    return audioFolder+constCantataCacheFile+MusicLibraryModel::constLibraryCompressedExt;
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

    // Remove old (non-compressed) cache file as well...
    QString oldCache=cacheFile;
    oldCache.replace(MusicLibraryModel::constLibraryCompressedExt, MusicLibraryModel::constLibraryExt);
    if (oldCache!=cacheFile && QFile::exists(oldCache)) {
        QFile::remove(oldCache);
    }
}
