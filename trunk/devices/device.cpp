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

#include "config.h"
#include "device.h"
#include "covers.h"
#include "utils.h"
#include <QtCore/QDir>

#ifndef Q_OS_WIN
#include "devicesmodel.h"
#include "umsdevice.h"
#ifdef MTP_FOUND
#include "mtpdevice.h"
#endif // MTP_FOUND
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
#else // ENABLE_KDE_SUPPORT
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
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

#ifndef Q_OS_WIN

#include <unistd.h>

const QLatin1String Device::constNoCover("-");

Device * Device::create(DevicesModel *m, const QString &udi)
{
    Solid::Device device=Solid::Device(udi);

    if (device.is<Solid::PortableMediaPlayer>())
    {
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
            model->beginRemoveRows(model->createIndex(model->devices.indexOf(this), 0, this), 0, oldCount-1);
            m_childItems.clear();
            qDeleteAll(m_childItems);
            model->endRemoveRows();
        }
        int newCount=newRows();
        if (newCount>0) {
            model->beginInsertRows(model->createIndex(model->devices.indexOf(this), 0, this), 0, newCount-1);
            foreach (MusicLibraryItem *item, update->childItems()) {
                item->setParent(this);
            }
            delete update;
            refreshIndexes();
            model->endInsertRows();
        }
    }
    update=0;
}

const MusicLibraryItem * Device::findSong(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = ((MusicLibraryItemRoot *)this)->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.title) {
                    return songItem;
                }
            }
        }
    }

    return 0;
}

bool Device::songExists(const Song &s) const
{
    const MusicLibraryItem *song=findSong(s);

    if (song) {
        return true;
    }

    if (!s.isVariousArtists()) {
        Song mod(s);
        mod.albumartist=i18n("Various Artists");
        if (MPDParseUtils::groupMultiple()) {
            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist()) {
                    return true;
                }
            }
        }
        if (MPDParseUtils::groupSingle()) {
            mod.album=i18n("Single Tracks");
            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist() && sng.album==s.album) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool Device::updateSong(const Song &orig, const Song &edit)
{
    if ((supportsAlbumArtist ? orig.albumArtist()==edit.albumArtist() : orig.artist==edit.artist) && orig.album==edit.album) {
        MusicLibraryItemArtist *artistItem = artist(orig, false);
        if (!artistItem) {
            return false;
        }
        MusicLibraryItemAlbum *albumItem = artistItem->album(orig, false);
        if (!albumItem) {
            return false;
        }
        int songRow=0;
        foreach (MusicLibraryItem *song, albumItem->childItems()) {
            if (static_cast<MusicLibraryItemSong *>(song)->song()==orig) {
                static_cast<MusicLibraryItemSong *>(song)->setSong(edit);
                QModelIndex idx=model->createIndex(songRow, 0, albumItem);
                emit model->dataChanged(idx, idx);
                return true;
            }
            songRow++;
        }
    }
    return false;
}

void Device::addSongToList(const Song &s)
{
    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        model->beginInsertRows(model->createIndex(model->devices.indexOf(this), 0, this), childCount(), childCount());
        artistItem = createArtist(s);
        model->endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        model->beginInsertRows(model->createIndex(childItems().indexOf(artistItem), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        model->endInsertRows();
    }
    quint32 year=albumItem->year();
    foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
        const MusicLibraryItemSong *song=static_cast<const MusicLibraryItemSong *>(songItem);
        if (song->track()==s.track && song->disc()==s.disc && song->data()==s.title) {
            return;
        }
    }

    model->beginInsertRows(model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    model->endInsertRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem);
        emit model->dataChanged(idx, idx);
    }
}

void Device::removeSongFromList(const Song &s)
{
    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        return;
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        return;
    }
    MusicLibraryItem *songItem=0;
    int songRow=0;
    foreach (MusicLibraryItem *song, albumItem->childItems()) {
        if (static_cast<MusicLibraryItemSong *>(song)->song().title==s.title) {
            songItem=song;
            break;
        }
        songRow++;
    }
    if (!songItem) {
        return;
    }

    if (1==artistItem->childCount() && 1==albumItem->childCount()) {
        // 1 album with 1 song - so remove whole artist
        int row=m_childItems.indexOf(artistItem);
        model->beginRemoveRows(model->createIndex(model->devices.indexOf(this), 0, this), row, row);
        remove(artistItem);
        model->endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=artistItem->childItems().indexOf(albumItem);
        model->beginRemoveRows(model->createIndex(childItems().indexOf(artistItem), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        model->endRemoveRows();
        return;
    }

    // Just remove particular song
    model->beginRemoveRows(model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), songRow, songRow);
    quint32 year=albumItem->year();
    albumItem->remove(songRow);
    model->endRemoveRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem);
        emit model->dataChanged(idx, idx);
    }
}

void Device::setStatusMessage(const QString &msg)
{
    statusMsg=msg;
    QModelIndex modelIndex=model->createIndex(model->devices.indexOf(this), 0, this);
    emit model->dataChanged(modelIndex, modelIndex);
}

void Device::songCount(int c)
{
    setStatusMessage(i18n("Updating (%1)...").arg(c));
}

#endif // Q_OS_WIN