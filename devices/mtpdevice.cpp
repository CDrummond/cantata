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

#include "mtpdevice.h"
#include "tagreader.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "covers.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>

MtpDevice::MtpDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev)
    , pmp(dev.as<Solid::PortableMediaPlayer>())
//     , scanner(0)
//     , propDlg(0)
{
    setup();
}

MtpDevice::~MtpDevice() {
    stopScanner();
}

void MtpDevice::connectTo()
{
    if (pmp && !isConnected()) {
//         if (pmp->setup()) {
//             emit connected(solidDev.udi());
//             setup();
//         }
    }
}

void MtpDevice::disconnectFrom()
{
    if (pmp && !isConnected()) {
//         if (pmp->teardown()) {
//             emit disconnected(solidDev.udi());
//         }
    }
}

bool MtpDevice::isConnected()
{
    return pmp /*&& pmp->isAccessible()*/;
}

void MtpDevice::configure(QWidget *parent)
{
//     if (!propDlg) {
//         propDlg=new DevicePropertiesDialog(parent);
//         connect(propDlg, SIGNAL(updatedSettings(const QString &, const Device::NameOptions &)), SLOT(saveProperties(const QString &, const Device::NameOptions &)));
//     }
//     propDlg->show(audioFolder, nameOpts);
}

void MtpDevice::rescan()
{
    if (isConnected()) {
        startScanner();
    }
}

void MtpDevice::addSong(const Song &s, bool overwrite)
{
//     if (!isConnected()) {
//         emit actionStatus(NotConnected);
//         return;
//     }
//
//     if (!overwrite && songExists(s)) {
//         emit actionStatus(SongExists);
//         return;
//     }
//
//     if (!QFile::exists(s.file)) {
//         emit actionStatus(SourceFileDoesNotExist);
//         return;
//     }
//
//     QString destFile=audioFolder+nameOpts.createFilename(s);
//
//     if (!overwrite && QFile::exists(destFile)) {
//         emit actionStatus(FileExists);
//         return;
//     }
//
//     KUrl dest(destFile);
//     QDir dir(dest.directory());
//     if( !dir.exists() && !dir.mkpath( "." ) ) {
//         emit actionStatus(DirCreationFaild);
//     }
//
//     currentSong=s;
//     KIO::FileCopyJob *job=KIO::file_copy(KUrl(s.file), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
//     connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *job)));
}

void MtpDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)
{
//     if (!isConnected()) {
//         emit actionStatus(NotConnected);
//         return;
//     }
//
//     if (!overwrite && MusicLibraryModel::self()->songExists(s)) {
//         emit actionStatus(SongExists);
//         return;
//     }
//
//     QString source=s.file; // Device files have full path!!!
//
//     if (!QFile::exists(source)) {
//         emit actionStatus(SourceFileDoesNotExist);
//         return;
//     }
//
//     if (!overwrite && QFile::exists(baseDir+musicPath)) {
//         emit actionStatus(FileExists);
//         return;
//     }
//
//     currentBaseDir=baseDir;
//     currentMusicPath=musicPath;
//     KUrl dest(currentBaseDir+currentMusicPath);
//     QDir dir(dest.directory());
//     if (!dir.exists() && !dir.mkpath( "." )) {
//         emit actionStatus(DirCreationFaild);
//     }
//
//     currentSong=s;
//     KIO::FileCopyJob *job=KIO::file_copy(KUrl(source), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
//     connect(job, SIGNAL(result(KJob *)), SLOT(copySongToResult(KJob *)));
}

void MtpDevice::removeSong(const Song &s)
{
//     if (!isConnected()) {
//         emit actionStatus(NotConnected);
//         return;
//     }
//
//     if (!QFile::exists(s.file)) {
//         emit actionStatus(SourceFileDoesNotExist);
//         return;
//     }
//
//     currentSong=s;
//     KIO::SimpleJob *job=KIO::file_delete(KUrl(s.file), KIO::HideProgressInfo);
//     connect(job, SIGNAL(result(KJob *)), SLOT(removeSongResult(KJob *job)));
}

// void MtpDevice::addSongResult(KJob *job)
// {
//     if (job->error()) {
//         emit actionStatus(Failed);
//     } else {
//         QString sourceDir=MPDParseUtils::getDir(currentSong.file);
//         QString destFile=audioFolder+nameOpts.createFilename(currentSong);
//
//         currentSong.file=destFile;
//         Covers::copyCover(currentSong, sourceDir, MPDParseUtils::getDir(currentSong.file), false);
//         addSongToList(currentSong);
//         emit actionStatus(Ok);
//     }
// }
//
// void MtpDevice::copySongToResult(KJob *job)
// {
//     if (job->error()) {
//         emit actionStatus(Failed);
//     } else {
//         QString sourceDir=MPDParseUtils::getDir(currentSong.file);
//         currentSong.file=currentMusicPath; // MPD's paths are not full!!!
//         Covers::copyCover(currentSong, sourceDir, currentBaseDir+MPDParseUtils::getDir(currentMusicPath), true);
//         MusicLibraryModel::self()->addSongToList(currentSong);
//         emit actionStatus(Ok);
//     }
// }
//
// void MtpDevice::removeSongResult(KJob *job)
// {
//     if (job->error()) {
//         emit actionStatus(Failed);
//     } else {
//         removeSongFromList(currentSong);
//         emit actionStatus(Ok);
//     }
// }

double MtpDevice::usedCapacity()
{
//     if (!isConnected()) {
        return -1.0;
//     }
//
//     KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(pmp->filePath());
//     return (inf.used()*1.0)/(inf.size()*1.0);
}

QString MtpDevice::capacityString()
{
//     if (!isConnected()) {
        return i18n("Not Connected");
//     }
//
//     KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(pmp->filePath());
//     return i18n("%1 Free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
}

qint64 MtpDevice::freeSpace()
{
//     if (!isConnected()) {
        return 0;
//     }
//
//     KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(pmp->filePath());
//     return inf.size()-inf.used();
}

void MtpDevice::setup()
{
    if (!isConnected()) {
        return;
    }

    rescan();
}

void MtpDevice::startScanner()
{
//     stopScanner();
//     scanner=new MusicScanner(audioFolder);
//     connect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
//     scanner->start();
//     emit updating(solidDev.udi(), true);
}

void MtpDevice::stopScanner()
{
    // Scan for music in a separate thread...
//     if (scanner) {
//         disconnect(scanner, SIGNAL(finished()), this, SLOT(libraryUpdated()));
//         scanner->deleteLater();
//         scanner->stop();
//         scanner=0;
//         emit updating(solidDev.udi(), false);
//     }
}

void MtpDevice::libraryUpdated()
{
//     if (scanner) {
//         if (update) {
//             delete update;
//         }
//         update=scanner->takeLibrary();
//         stopScanner();
//     }
}
