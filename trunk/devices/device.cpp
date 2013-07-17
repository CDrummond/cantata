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

#include "config.h"
#include "device.h"
#include "covers.h"
#include "utils.h"
#include <QBuffer>
#include <QDir>
#include <QTemporaryFile>
#include <QTimer>
#ifndef Q_OS_WIN
#include "devicesmodel.h"
#include "umsdevice.h"
#ifdef MTP_FOUND
#include "mtpdevice.h"
#endif // MTP_FOUND
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocddevice.h"
#endif // defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "encoders.h"
#include "tags.h"
#include "song.h"
#include "mpdparseutils.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "localize.h"
#ifdef ENABLE_KDE_SUPPORT
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/opticaldisc.h>
#else // ENABLE_KDE_SUPPORT
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
#include "solid-lite/opticaldisc.h"
#endif // ENABLE_KDE_SUPPORT
#endif // Q_OS_WIN

void Device::moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile)
{
    QDir d(from);
    if (d.exists()) {
        QFileInfoList entries=d.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Dirs|QDir::NoDotAndDotDot);
        QList<QString> extraFiles;
        QSet<QString> others=Covers::standardNames().toSet();
        others << coverFile << "albumart.pamp";

        foreach (const QFileInfo &info, entries) {
            if (info.isDir()) {
                return;
            }
            if (!others.contains(info.fileName())) {
                return;
            }
            extraFiles.append(info.fileName());
        }

        foreach (const QString &cf, extraFiles) {
            if (!QFile::rename(from+'/'+cf, to+'/'+cf)) {
                return;
            }
        }
        cleanDir(from, base, coverFile);
    }
}

void Device::cleanDir(const QString &dir, const QString &base, const QString &coverFile, int level)
{
    QDir d(dir);
    if (d.exists()) {
        QFileInfoList entries=d.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Dirs|QDir::NoDotAndDotDot);
        QList<QString> extraFiles;
        QSet<QString> others=Covers::standardNames().toSet();
        others << coverFile << "albumart.pamp";

        foreach (const QFileInfo &info, entries) {
            if (info.isDir()) {
                return;
            }
            if (!others.contains(info.fileName())) {
                return;
            }
            extraFiles.append(info.absoluteFilePath());
        }

        foreach (const QString &cf, extraFiles) {
            if (!QFile::remove(cf)) {
                return;
            }
        }

        if (Utils::fixPath(dir)==Utils::fixPath(base)) {
            return;
        }
        QString dirName=d.dirName();
        if (dirName.isEmpty()) {
            return;
        }
        d.cdUp();
        if (!d.rmdir(dirName)) {
            return;
        }
        if (level>=3) {
            return;
        }
        QString upDir=d.absolutePath();
        if (Utils::fixPath(upDir)!=Utils::fixPath(base)) {
            cleanDir(upDir, base, coverFile, level+1);
        }
    }
}

Song Device::fixPath(const Song &orig, bool fullPath) const
{
    Song s=orig;
    if (fullPath) {
        QString p=path();
        if (!p.isEmpty()) {
            s.file=p+s.file;
        }
    }
    return s;
}

#ifndef Q_OS_WIN

#include <unistd.h>

const QLatin1String Device::constNoCover("-");
const QLatin1String Device::constEmbedCover("+");

Device * Device::create(MusicModel *m, const QString &udi)
{
    Solid::Device device=Solid::Device(udi);

    if (device.is<Solid::OpticalDisc>()) {
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        return Encoders::getAvailable().isEmpty() ? 0 : new AudioCdDevice(m, device);
        #endif
    } else if (device.is<Solid::PortableMediaPlayer>()) {
        #ifdef MTP_FOUND
        Solid::PortableMediaPlayer *pmp = device.as<Solid::PortableMediaPlayer>();

        if (pmp->supportedProtocols().contains(QLatin1String("mtp"))) {
            return new MtpDevice(m, device);
        }
        #endif
    } else if (device.is<Solid::StorageAccess>()) {

        const Solid::StorageAccess* ssa = device.as<Solid::StorageAccess>();

        if( ssa && (!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
                   (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus()) ) {
            return 0;
        }

        //HACK: ignore apple stuff until we have common MediaDeviceFactory.
        if (!device.vendor().contains("apple", Qt::CaseInsensitive)) {
//             Solid::StorageAccess *sa = device.as<Solid::StorageAccess>();
//             if (QLatin1String("usb")==sa->bus) {
                return new UmsDevice(m, device);
//             }
        }
    }
    return 0;
}

bool Device::fixVariousArtists(const QString &file, Song &song, bool applyFix)
{
    Song orig=song;
    if (!file.isEmpty() && song.albumartist.isEmpty()) {
        song=Tags::read(file);
    }

    if (song.artist.isEmpty() || song.albumartist.isEmpty() || !Song::isVariousArtists(song.albumartist)) {
        song=orig;
        return false;
    }

    bool needsUpdating=false;

    if (!applyFix) { // Then real artist is embedded in track title...
        needsUpdating=song.revertVariousArtists();
    } else if (applyFix) { // We must be copying to device, and need to place song artist into title...
        needsUpdating=song.fixVariousArtists();
    }

    if (needsUpdating && (file.isEmpty() || Tags::Update_Modified==Tags::updateArtistAndTitle(file, song))) {
        return true;
    }
    song=orig;
    return false;
}

static QByteArray save(const QImage &img) {
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPG");
    buffer.close();
    return ba;
}

void Device::embedCover(const QString &file, Song &song, unsigned int coverMaxSize)
{
    if (Tags::readImage(file).isNull()) {
        Covers::Image coverImage=Covers::self()->getImage(song);
        if (!coverImage.img.isNull()) {
            QByteArray imgData;
            if (coverMaxSize && (coverImage.img.width()>(int)coverMaxSize || coverImage.img.height()>(int)coverMaxSize)) {
                imgData=save(coverImage.img.scaled(QSize(coverMaxSize, coverMaxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else if (!coverImage.fileName.endsWith(".jpg", Qt::CaseInsensitive) || !QFile::exists(coverImage.fileName)) {
                imgData=save(coverImage.img);
            } else {
                QFile f(coverImage.fileName);
                if (f.open(QIODevice::ReadOnly)) {
                    imgData=f.readAll();
                } else {
                    imgData=save(coverImage.img);
                }
            }
            Tags::embedImage(file, imgData);
        }
    }
}

QTemporaryFile * Device::copySongToTemp(Song &song)
{
    QTemporaryFile *temp=new QTemporaryFile();

    int index=song.file.lastIndexOf('.');
    if (index>0) {
        temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX"+song.file.mid(index));
    } else {
        temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX");
    }

    temp->setAutoRemove(false);
    if (temp->open()) {
        temp->close();
        if (QFile::exists(temp->fileName())) {
            QFile::remove(temp->fileName()); // Copy will *not* overwrite file!
        }
        if (!QFile::copy(song.file, temp->fileName())) {
            temp->remove();
            temp=0;
        }
    }
    return temp;
}

void Device::saveCache()
{
    QTimer::singleShot(0, this, SIGNAL(cacheSaved()));
}

void Device::applyUpdate()
{
    if (!update) {
        return;
    }
    /*if (m_childItems.count() && update && update->childCount()) {
        QSet<Song> currentSongs=allSongs();
        QSet<Song> updateSongs=update->allSongs();
        QSet<Song> removed=currentSongs-updateSongs;
        QSet<Song> added=updateSongs-currentSongs;

        foreach (const Song &s, removed) {
            removeSongFromList(s);
        }
        foreach (const Song &s, added) {
            addSongToList(s);
        }
        delete update;
    } else*/ {
        int oldCount=childCount();
        if (oldCount>0) {
            m_model->beginRemoveRows(index(), 0, oldCount-1);
            clearItems();
            m_model->endRemoveRows();
        }
        int newCount=newRows();
        if (newCount>0) {
            m_model->beginInsertRows(index(), 0, newCount-1);
            foreach (MusicLibraryItem *item, update->childItems()) {
                item->setParent(this);
            }
            if (AudioCd!=devType()) {
                refreshIndexes();
            }
            m_model->endInsertRows();
        }
    }
    delete update;
    update=0;
}

QModelIndex Device::index() const
{
    return m_model->createIndex(m_model->row((void *)this), 0, (void *)this);
}

void Device::setStatusMessage(const QString &msg)
{
    statusMsg=msg;
    updateStatus();
}

void Device::updateStatus()
{
    QModelIndex modelIndex=index();
    emit m_model->dataChanged(modelIndex, modelIndex);
}

void Device::songCount(int c)
{
    setStatusMessage(i18n("Updating (%1)...").arg(c));
}

#endif // Q_OS_WIN
