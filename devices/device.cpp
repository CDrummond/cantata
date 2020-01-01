/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "gui/covers.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include <QBuffer>
#include <QDir>
#include <QTemporaryFile>
#include <QTimer>
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#include "umsdevice.h"
#ifdef MTP_FOUND
#include "mtpdevice.h"
#endif // MTP_FOUND
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocddevice.h"
#endif // defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "encoders.h"
#include "tags/tags.h"
#include "mpd-interface/song.h"
#include "mpd-interface/mpdparseutils.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/musiclibraryitemsong.h"
#include "models/musiclibrarymodel.h"
#include "widgets/icons.h"
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
#include "solid-lite/opticaldisc.h"
#include "solid-lite/genericinterface.h"
#endif // ENABLE_DEVICES_SUPPORT

#include <QDebug>
#define DBUG_CLASS(CLASS) if (DevicesModel::debugEnabled()) qWarning() << CLASS << __FUNCTION__
#define DBUG DBUG_CLASS(metaObject()->className())

void Device::moveDir(const QString &from, const QString &to, const QString &base, const QString &coverFile)
{
    QDir d(from);
    if (d.exists()) {
        QFileInfoList entries=d.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Dirs|QDir::NoDotAndDotDot);
        QSet<QString> fileTypes=QSet<QString>() << "jpg" << "png" << "pamp" << "txt" << "lyrics";

        for (const QFileInfo &info: entries) {
            if (!info.isDir() && (MPDConnection::isPlaylist(info.fileName()) || fileTypes.contains(info.suffix().toLower()))) {
                QFile::rename(from+'/'+info.fileName(), to+'/'+info.fileName());
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

        for (const QFileInfo &info: entries) {
            if (info.isDir()) {
                return;
            }
            if (!others.contains(info.fileName())) {
                return;
            }
            extraFiles.append(info.absoluteFilePath());
        }

        for (const QString &cf: extraFiles) {
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

#ifdef ENABLE_DEVICES_SUPPORT

#include <unistd.h>

const QLatin1String Device::constNoCover("-");
const QLatin1String Device::constEmbedCover("+");

Device * Device::create(MusicLibraryModel *m, const QString &udi)
{
    Solid::Device device=Solid::Device(udi);

    DBUG_CLASS("Device") << udi;
    if (device.is<Solid::OpticalDisc>()) {
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        DBUG_CLASS("Device") << "AudioCD";
        return Encoders::getAvailable().isEmpty() ? 0 : new AudioCdDevice(m, device);
        #endif
    } else if (device.is<Solid::PortableMediaPlayer>()) {
        #ifdef MTP_FOUND
        Solid::PortableMediaPlayer *pmp = device.as<Solid::PortableMediaPlayer>();

        if (pmp->supportedProtocols().contains(QLatin1String("mtp"))) {

            Solid::GenericInterface *iface = device.as<Solid::GenericInterface>();

            if (iface) {
                QMap<QString, QVariant> properties = iface->allProperties();
                QMap<QString, QVariant>::ConstIterator bus = properties.find(QLatin1String("BUSNUM"));
                QMap<QString, QVariant>::ConstIterator dev = properties.find(QLatin1String("DEVNUM"));

                DBUG_CLASS("Device") << "MTP";
                return properties.constEnd()==bus || properties.end()==dev
                        ? nullptr
                        : new MtpDevice(m, device, bus.value().toUInt(), dev.value().toUInt());
            }
        }
        #endif
    } else if (device.is<Solid::StorageAccess>()) {
        DBUG_CLASS("Device") << "Storage";

        if ( (!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
             (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus()) ) {
            DBUG_CLASS("Device") << "Not drive / usb ?";
            return nullptr;
        }

        DBUG_CLASS("Device") << "UMS, Vendor:" << device.vendor();
        return new UmsDevice(m, device);
    }
    return nullptr;
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
        temp=new QTemporaryFile("cantata_XXXXXX"+song.file.mid(index));
    } else {
        temp=new QTemporaryFile("cantata_XXXXXX");
    }

    temp->setAutoRemove(false);
    if (temp->open()) {
        temp->close();
        if (QFile::exists(temp->fileName())) {
            QFile::remove(temp->fileName()); // Copy will *not* overwrite file!
        }
        if (!QFile::copy(song.file, temp->fileName())) {
            temp->remove();
            delete temp;
            temp=nullptr;
        }
    }
    return temp;
}

bool Device::isLossless(const QString &file)
{
    static QStringList lossless=QStringList() << ".flac" << ".wav"; // TODO: ALAC?? Its in .m4a!!!
    QString lower = file.toLower();
    for (const auto &e: lossless) {
        if (lower.endsWith(e)) {
            return true;
        }
    }
    return false;
}

Device::Device(MusicLibraryModel *m, Solid::Device &dev, bool albumArtistSupport, bool flat)
    : MusicLibraryItemRoot(dev.product().startsWith(dev.vendor()) ? dev.product() : (dev.vendor()+QChar(' ')+dev.product()), albumArtistSupport, flat)
    , configured(false)
    , solidDev(dev)
    , deviceId(dev.udi())
    , update(nullptr)
    , needToFixVa(false)
    , jobAbortRequested(false)
    , transcoding(false)
{
    m_model=m;
    icn=Icons::self()->folderListIcon;
    m_itemData[0]=m_itemData[0].toUpper();
}

Device::Device(MusicLibraryModel *m, const QString &name, const QString &id)
    : MusicLibraryItemRoot(name)
    , configured(false)
    , deviceId(id)
    , update(nullptr)
    , needToFixVa(false)
    , jobAbortRequested(false)
    , transcoding(false)
{
    m_model=m;
    icn=solidDev.isValid() ? Icon::get(solidDev.icon()) : Icons::self()->folderListIcon;
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

        for (const Song &s: removed) {
            removeSongFromList(s);
        }
        for (const Song &s: added) {
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
    update=nullptr;
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
    setStatusMessage(tr("Updating (%1)...").arg(c));
}

void Device::updatePercentage(int pc)
{
    setStatusMessage(tr("Updating (%1%)...").arg(pc));
}

#endif // ENABLE_DEVICES_SUPPORT

#include "moc_device.cpp"
