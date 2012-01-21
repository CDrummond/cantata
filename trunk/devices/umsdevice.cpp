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
#include "tagreader.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "devicepropertiesdialog.h"
#include "covers.h"
#include "mpdparseutils.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <KDE/KUrl>
#include <KDE/KDiskFreeSpaceInfo>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KIO/FileCopyJob>
#include <KDE/KIO/Job>

static const QLatin1String constSettingsFile("/.is_audio_player");
static const QLatin1String constMusicFolderKey("audio_folder");
// static const QLatin1String constPodcastFolderKey("podcast_folder");
static const QLatin1String constMusicFilenameSchemeKey("music_filenamescheme");
static const QLatin1String constVfatSafeKey("vfat_safe");
static const QLatin1String constAsciiOnlyKey("ascii_only");
static const QLatin1String constIgnoreTheKey("ignore_the");
static const QLatin1String constReplaceSpacesKey("replace_spaces");
// static const QLatin1String constUseAutomaticallyKey("use_automatically");
static const QLatin1String constCoverFileName("cover_filename"); // Cantata extension!
static const QLatin1String constVariousArtistsFix("fix_various_artists"); // Cantata extension!
static const QLatin1String constDefCoverFileName("cover.jpg");

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
        foreach (const QFileInfo &info, entries) {
            if (stopRequested) {
                return;
            }
            if (info.isDir()) {
                scanFolder(info.absoluteFilePath(), level+1);
            } else if(info.isReadable()) {
                Song song=TagReader::read(info.absoluteFilePath());

                if (song.isEmpty()) {
                    continue;
                }

                song.fillEmptyFields();
                song.size=info.size();
                MusicLibraryItemArtist *artistItem = library->artist(song);
                MusicLibraryItemAlbum *albumItem = artistItem->album(song);
                MusicLibraryItemSong *songItem = new MusicLibraryItemSong(song, albumItem);

                albumItem->append(songItem);
                albumItem->addGenre(song.genre);
                artistItem->addGenre(song.genre);
                songItem->addGenre(song.genre);
                library->addGenre(song.genre);
            }
        }
    }
}

UmsDevice::UmsDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev)
    , access(dev.as<Solid::StorageAccess>())
    , scanner(0)
{
    setup();
}

UmsDevice::~UmsDevice() {
    stopScanner();
}

bool UmsDevice::isConnected() const
{
    return access && access->isAccessible();
}

void UmsDevice::configure(QWidget *parent)
{
    DevicePropertiesDialog *dlg=new DevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const QString &, const Device::Options &)),
            SLOT(saveProperties(const QString &, const QString &, const Device::Options &)));
    dlg->show(audioFolder, coverFileName, opts, DevicePropertiesDialog::Prop_All);
}

void UmsDevice::rescan()
{
    if (isConnected()) {
        startScanner();
    }
}

void UmsDevice::addSong(const Song &s, bool overwrite)
{
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

    if (!overwrite && QFile::exists(destFile)) {
        emit actionStatus(FileExists);
        return;
    }

    KUrl dest(destFile);
    QDir dir(dest.directory());
    if(!dir.exists() && !Device::createDir(dir.absolutePath())) {
        emit actionStatus(DirCreationFaild);
    }

    currentSong=s;
    KIO::FileCopyJob *job=KIO::file_copy(KUrl(s.file), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
    connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *)));
    connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(percent(KJob *, unsigned long)));
}

void UmsDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)
{
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
    if (!dir.exists() && !Device::createDir(dir.absolutePath())) {
        emit actionStatus(DirCreationFaild);
    }

    currentSong=s;
    KIO::FileCopyJob *job=KIO::file_copy(KUrl(source), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
    connect(job, SIGNAL(result(KJob *)), SLOT(copySongToResult(KJob *)));
    connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(percent(KJob *, unsigned long)));
}

void UmsDevice::removeSong(const Song &s)
{
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

void UmsDevice::percent(KJob *job, unsigned long percent)
{
    Q_UNUSED(job)
    emit progress(percent);
}

void UmsDevice::addSongResult(KJob *job)
{
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        QString sourceDir=MPDParseUtils::getDir(currentSong.file);
        QString destFile=audioFolder+opts.createFilename(currentSong);

        currentSong.file=destFile;
        Covers::copyCover(currentSong, sourceDir, MPDParseUtils::getDir(currentSong.file), coverFileName);

        if (needToFixVa) {
            Device::fixVariousArtists(destFile, currentSong, true);
        }
        addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void UmsDevice::copySongToResult(KJob *job)
{
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        Device::setFilePerms(currentSong.file);
        QString sourceDir=MPDParseUtils::getDir(currentSong.file);
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        Covers::copyCover(currentSong, sourceDir, currentBaseDir+MPDParseUtils::getDir(currentMusicPath), QString());
        if (needToFixVa) {
            Device::fixVariousArtists(currentBaseDir+currentSong.file, currentSong, false);
        }
        MusicLibraryModel::self()->addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void UmsDevice::removeSongResult(KJob *job)
{
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
}

double UmsDevice::usedCapacity()
{
    if (!isConnected()) {
        return -1.0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(access->filePath());
    return inf.size()>0 ? (inf.used()*1.0)/(inf.size()*1.0) : -1.0;
}

QString UmsDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(access->filePath());
    return i18n("%1 free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
}

qint64 UmsDevice::freeSpace()
{
    if (!isConnected()) {
        return 0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(access->filePath());
    return inf.size()-inf.used();
}

void UmsDevice::setup()
{
    if (!isConnected()) {
        return;
    }

    QString path=access->filePath();
    if (path.isEmpty()) {
        return;
    }

    if (!path.endsWith('/')) {
        path+='/';
    }

    audioFolder = path;

    QFile file(path+constSettingsFile);
    QString audioFolderSetting;

    opts=Device::Options();
//     podcastFolder=QString();
    if (file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(constMusicFolderKey+"=")) {
                KUrl url = KUrl(path);
                url.addPath(line.section('=', 1, 1));
                url.cleanPath();
                audioFolderSetting=audioFolder=url.toLocalFile();
                if (!QDir(audioFolder).exists()) {
                    audioFolder = path;
                }
//             } else if (line.startsWith(constPodcastFolderKey+"=")) {
//                 podcastFolder=line.section('=', 1, 1);
            } else if (line.startsWith(constMusicFilenameSchemeKey+"=")) {
                QString scheme = line.section('=', 1, 1);
                //protect against empty setting.
                if( !scheme.isEmpty() ) {
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
//             } else if (line.startsWith(constUseAutomaticallyKey+"="))  {
//                 useAutomatically = QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constCoverFileName+"=")) {
                coverFileName=line.section('=', 1, 1);
                configured=true;
            } else if(line.startsWith(constVariousArtistsFix+"=")) {
                opts.fixVariousArtists=QLatin1String("true")==line.section('=', 1, 1);
                configured=true;
            } else {
                unusedParams+=line;
            }
        }
    }

    if (coverFileName.isEmpty()) {
        coverFileName=constDefCoverFileName;
    }

    // No setting, see if any standard dirs exist in path...
    if (audioFolderSetting.isEmpty() || audioFolderSetting!=audioFolder) {
        QStringList dirs;
        dirs << QLatin1String("Music") << QLatin1String("MUSIC")
             << QLatin1String("Albums") << QLatin1String("ALBUMS");

        foreach (const QString &d, dirs) {
            if (QDir(path+d).exists()) {
                audioFolder=path+d;
                break;
            }
        }
    }

    if (!audioFolder.endsWith('/')) {
        audioFolder+='/';
    }

    rescan();
}

void UmsDevice::startScanner()
{
    stopScanner();
    scanner=new MusicScanner(audioFolder);
    connect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
    scanner->start();
    setStatusMessage(i18n("Updating..."));
    emit updating(solidDev.udi(), true);
}

void UmsDevice::stopScanner()
{
    // Scan for music in a separate thread...
    if (scanner) {
        disconnect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
        scanner->deleteLater();
        scanner->stop();
        scanner=0;
        setStatusMessage(QString());
        emit updating(solidDev.udi(), false);
    }
}

void UmsDevice::libraryUpdated()
{
    if (scanner) {
        if (update) {
            delete update;
        }
        update=scanner->takeLibrary();
        stopScanner();
    }
}

void UmsDevice::saveProperties(const QString &newPath, const QString &newCoverFileName, const Device::Options &newOpts)
{
    if (configured && opts==newOpts && newPath==audioFolder && newCoverFileName==coverFileName) {
        return;
    }

    configured=true;
    opts=newOpts;
    QString oldPath=audioFolder;
    if (!isConnected()) {
        return;
    }

    QString path=access->filePath();
    if (path.isEmpty()) {
        return;
    }

    if (!path.endsWith('/')) {
        path+='/';
    }

    QFile file(path+constSettingsFile);
    QString fixedPath(newPath);

    if (fixedPath.startsWith(path)) {
        fixedPath=QLatin1String("./")+fixedPath.mid(path.length());
    }

    Options def;
    if (file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream out(&file);
        if (!fixedPath.isEmpty()) {
            out << constMusicFolderKey << '=' << fixedPath << '\n';
        }
//         if (!podcastFolder.isEmpty()) {
//             out << constPodcastFolderKey << '=' << podcastFolder << '\n';
//         }
        if (opts.scheme!=def.scheme) {
            out << constMusicFilenameSchemeKey << '=' << opts.scheme << '\n';
        }
        if (opts.scheme!=def.scheme) {
            out << constVfatSafeKey << '=' << (opts.vfatSafe ? "true" : "false") << '\n';
        }
        if (opts.asciiOnly!=def.asciiOnly) {
            out << constAsciiOnlyKey << '=' << (opts.asciiOnly ? "true" : "false") << '\n';
        }
        if (opts.ignoreThe!=def.ignoreThe) {
            out << constIgnoreTheKey << '=' << (opts.ignoreThe ? "true" : "false") << '\n';
        }
        if (opts.replaceSpaces!=def.replaceSpaces) {
            out << constReplaceSpacesKey << '=' << (opts.replaceSpaces ? "true" : "false") << '\n';
        }
//         if (useAutomatically) {
//             out << constUseAutomaticallyKey << '=' << (useAutomatically ? "true" : "false") << '\n';
//         }

        foreach (const QString &u, unusedParams) {
            out << u << '\n';
        }
        if (coverFileName!=constDefCoverFileName) {
            out << constCoverFileName << '=' << coverFileName << '\n';
        }
        if(opts.fixVariousArtists) {
            out << constVariousArtistsFix << '=' << (opts.fixVariousArtists ? "true" : "false") << '\n';
        }
    }

    if (oldPath!=newPath) {
        audioFolder=newPath;
        if (!audioFolder.endsWith('/')) {
            audioFolder+='/';
        }
        rescan();
    }
}
