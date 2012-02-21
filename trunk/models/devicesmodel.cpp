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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "devicesmodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "covers.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "mediadevicecache.h"
#include "umsdevice.h"
#include <QtGui/QMenu>
#include <QtCore/QStringList>
#include <QtCore/QMimeData>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#include <KDE/KLocale>
#include <KDE/KGlobal>
#endif

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(DevicesModel, instance)
#endif

DevicesModel * DevicesModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static DevicesModel *instance=0;;
    if(!instance) {
        instance=new DevicesModel;
    }
    return instance;
    #endif
}

DevicesModel::DevicesModel(QObject *parent)
    : QAbstractItemModel(parent)
    , itemMenu(0)
    , enabled(false)
{
    connect(MediaDeviceCache::self(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
    updateItemMenu();
}

DevicesModel::~DevicesModel()
{
    qDeleteAll(devices);
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesModel::loadRemote()
{
    QList<Device *> rem=RemoteDevice::loadAll(this);
    if (rem.count()) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count()+(rem.count()-1));
        foreach (Device *dev, rem) {
            indexes.insert(dev->udi(), devices.count());
            devices.append(dev);
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            connect(static_cast<RemoteDevice *>(dev), SIGNAL(udiChanged(const QString &, const QString &)), SLOT(changeDeviceUdi(const QString &, const QString &)));
        }
        endInsertRows();
    }
}

void DevicesModel::unmountRemote()
{
    foreach (Device *dev, devices) {
        if (Device::Remote==dev->type()) {
            static_cast<RemoteDevice *>(dev)->unmount();
        }
    }
}
#endif

QModelIndex DevicesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        MusicLibraryItem *p=static_cast<MusicLibraryItem *>(parent.internalPointer());

        if (p) {
            return row<p->children().count() ? createIndex(row, column, p->child(row)) : QModelIndex();
        }
    } else {
        return row<devices.count() ? createIndex(row, column, devices.at(row)) : QModelIndex();
    }

    return QModelIndex();
}

QModelIndex DevicesModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    MusicLibraryItem *childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem *parentItem = childItem->parent();

    if (parentItem) {
        return createIndex(parentItem->parent() ? parentItem->row() : devices.indexOf(static_cast<Device *>(parentItem)), 0, parentItem);
    } else {
        return QModelIndex();
    }
}

QVariant DevicesModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int DevicesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    return parent.isValid() ? static_cast<MusicLibraryItem *>(parent.internalPointer())->childCount() : devices.count();
}

int DevicesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant DevicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->type()) {
        case MusicLibraryItem::Type_Root: {
            QString iconName = static_cast<MusicLibraryItemRoot *>(item)->icon();
            return QIcon::fromTheme(iconName.isEmpty() ? QLatin1String("multimedia-player") : iconName);
        }
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            return artist->isVarious() ? QIcon::fromTheme("cantata-view-media-artist-various") : QIcon::fromTheme("view-media-artist");
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return QIcon::fromTheme("media-optical-audio");
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->type()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (static_cast<MusicLibraryItemAlbum *>(song->parent())->isSingleTracks()) {
                return song->song().artistSong();
            } else if (song->track()>9) {
                return QString::number(song->track())+QLatin1String(" - ")+item->data();
            } else if (song->track()>0) {
                return QChar('0')+QString::number(song->track())+QLatin1String(" - ")+item->data();
            }
        } else if(MusicLibraryItem::Type_Album==item->type() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->type()) {
        case MusicLibraryItem::Type_Root:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Artist", "%1\n%2 Artists", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1\n%2 Artists").arg(item->data()).arg(item->childCount())
                        : tr("%1\n1 Artist").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Artist:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Album", "%1\n%2 Albums", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1\n%2 Albums").arg(item->data()).arg(item->childCount())
                        : tr("%1\n1 Album").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Album:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Track", "%1\n%2 Tracks", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1\n%2 Tracks").arg(item->data()).arg(item->childCount())
                        : tr("%1\n1 Track").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Song: {
            QString duration=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(3);
            }
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(1);
            }
            return data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+duration+
                   QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        }
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Album==item->type()) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->type()) {
        case MusicLibraryItem::Type_Root: {
            Device *dev=static_cast<Device *>(item);

            if (!dev->statusMessage().isEmpty()) {
                return dev->statusMessage();
            }
            if (!dev->isConnected()) {
                return i18n("Not Connected");
            }
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Artist", "%1 Artists", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Artist") : tr("%1 Artists").arg(item->childCount());
            #endif
            break;
        }
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Album") : tr("%1 Albums").arg(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song: {
            QString text=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(3);
            }
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(1);
            }
            return text.mid(1);
        }
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track", "%1 Tracks", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Track") : tr("%1 Tracks").arg(item->childCount());
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->type()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        }
        return QVariant();
    case ItemView::Role_Capacity:
        if (MusicLibraryItem::Type_Root==item->type()) {
            return static_cast<Device *>(item)->usedCapacity();
        }
        return QVariant();
    case ItemView::Role_CapacityText:
        if (MusicLibraryItem::Type_Root==item->type()) {
            return static_cast<Device *>(item)->capacityString();
        }
        return QVariant();
    case ItemView::Role_ToggleIconName:
        #ifdef ENABLE_REMOTE_DEVICES
        if (MusicLibraryItem::Type_Root==item->type() && Device::Remote==static_cast<Device *>(item)->type()) {
            return QLatin1String(static_cast<Device *>(item)->isConnected() ? "network-connect" : "network-disconnect");
        }
        #endif
        return QVariant();
    default:
        return QVariant();
    }
    return QVariant();
}

void DevicesModel::clear()
{
    beginResetModel();
    qDeleteAll(devices);
    devices.clear();
    indexes.clear();
    endResetModel();
    updateItemMenu();
//     emit updated(rootItem);
    emit updateGenres(QSet<QString>());
}

void DevicesModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    enabled=e;

    if (enabled) {
        connect(MediaDeviceCache::self(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)),
                this, SLOT(setCover(const QString &, const QString &, const QImage &, const QString &)));
        MediaDeviceCache::self()->refreshCache();
        QStringList deviceUdiList=MediaDeviceCache::self()->getAll();
        foreach (const QString &udi, deviceUdiList) {
            deviceAdded(udi);
        }
        if (devices.isEmpty()) {
            updateItemMenu();
        }
    } else {
        disconnect(MediaDeviceCache::self(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        disconnect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)),
                   this, SLOT(setCover(const QString &, const QString &, const QImage &, const QString &)));
    }
}

Device * DevicesModel::device(const QString &udi)
{
    if (indexes.contains(udi)) {
        return devices.at(indexes[udi]);
    }
    return 0;
}

void DevicesModel::setCover(const QString &artist, const QString &album, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    if (img.isNull()) {
        return;
    }

    Song song;
    song.artist=artist;
    song.album=album;
    int i=0;
    foreach (MusicLibraryItem *device, devices) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemRoot *>(device)->artist(song, false);
        if (artistItem) {
            MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
            if (albumItem) {
                if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
                    QModelIndex idx=index(artistItem->children().indexOf(albumItem), 0, index(device->children().indexOf(artistItem), 0, index(i, 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
        i++;
    }

//     foreach (const MusicLibraryItem *device, devices) {
//         bool found=false;
//         int j=0;
//         foreach (const MusicLibraryItem *artistItem, device->children()) {
//             if (artistItem->data()==artist) {
//                 int k=0;
//                 foreach (const MusicLibraryItem *albumItem, artistItem->children()) {
//                     if (albumItem->data()==album) {
//                         if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
//                             QModelIndex idx=index(k, 0, index(j, 0, index(i, 0, QModelIndex())));
//                             emit dataChanged(idx, idx);
//                         }
//                         found=true;
//                         break;
//                     }
//                     k++;
//                 }
//                 if (found) {
//                     break;
//                 }
//             }
//             j++;
//         }
//         i++;
//     }
}

void DevicesModel::deviceAdded(const QString &udi)
{
    if (!indexes.contains(udi)) {
        Device *dev=Device::create(this, udi);
        if (dev) {
            beginInsertRows(QModelIndex(), devices.count(), devices.count());
            indexes.insert(udi, devices.count());
            devices.append(dev);
            endInsertRows();
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            updateItemMenu();
        }
    }
}

void DevicesModel::deviceRemoved(const QString &udi)
{
    if (indexes.contains(udi)) {
        int idx=indexes[udi];
        beginRemoveRows(QModelIndex(), idx, idx);
        delete devices.takeAt(idx);
        indexes.remove(udi);
        endRemoveRows();
        updateItemMenu();
    }
}

void DevicesModel::deviceUpdating(const QString &udi, bool state)
{
    if (indexes.contains(udi)) {
        int idx=indexes[udi];
        Device *dev=devices.at(idx);

        if (state) {
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
        } else {
            if (dev->haveUpdate()) {
                dev->applyUpdate();
            }
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
            QSet<QString> genres;
            foreach (Device *dev, devices) {
                genres+=dev->genres();
            }
            emit updateGenres(genres);
        }
    }
}

Qt::ItemFlags DevicesModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    }
    return Qt::ItemIsEnabled;
}

QStringList DevicesModel::filenames(const QModelIndexList &indexes, bool umsOnly) const
{
    QStringList fnames;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        if (!umsOnly) {
            MusicLibraryItem *parent=item;
            while (parent->parent()) {
                parent=parent->parent();
            }
            if (parent && Device::Ums!=static_cast<Device *>(parent)->type()) {
                continue;
            }
        }

        switch (item->type()) {
        case MusicLibraryItem::Type_Root:
            foreach (const MusicLibraryItem *artist, item->children()) {
                foreach (const MusicLibraryItem *album, artist->children()) {
                    QStringList sorted=static_cast<const MusicLibraryItemAlbum *>(album)->sortedTracks();
                    foreach (const QString &f, sorted) {
                        if(!fnames.contains(f)) {
                            fnames << f;
                        }
                    }
                }
            }
            break;
        case MusicLibraryItem::Type_Artist:
            foreach (const MusicLibraryItem *album, item->children()) {
                QStringList sorted=static_cast<const MusicLibraryItemAlbum *>(album)->sortedTracks();
                foreach (const QString &f, sorted) {
                    if(!fnames.contains(f)) {
                        fnames << f;
                    }
                }
            }
            break;
        case MusicLibraryItem::Type_Album: {
            QStringList sorted=static_cast<MusicLibraryItemAlbum*>(item)->sortedTracks();
                foreach (const QString &f, sorted) {
                    if(!fnames.contains(f)) {
                        fnames << f;
                    }
                }
            break;
        }
        case MusicLibraryItem::Type_Song:
            if (item->type() == MusicLibraryItem::Type_Song && !fnames.contains(static_cast<MusicLibraryItemSong*>(item)->file())) {
                fnames << static_cast<MusicLibraryItemSong*>(item)->file();
            }
            break;
        default:
            break;
        }
    }
    return fnames;
}

QList<Song> DevicesModel::songs(const QModelIndexList &indexes) const
{
    QList<Song> songs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Root:
            foreach (const MusicLibraryItem *artist, item->children()) {
                foreach (const MusicLibraryItem *album, artist->children()) {
                    foreach (MusicLibraryItem *song, album->children()) {
                        if (MusicLibraryItem::Type_Song==song->type() && !songs.contains(static_cast<MusicLibraryItemSong*>(song)->song())) {
                            songs << static_cast<MusicLibraryItemSong*>(song)->song();
                        }
                    }
                }
            }
            break;
        case MusicLibraryItem::Type_Artist:
            foreach (const MusicLibraryItem *album, item->children()) {
                foreach (MusicLibraryItem *song, album->children()) {
                    if (MusicLibraryItem::Type_Song==song->type() && !songs.contains(static_cast<MusicLibraryItemSong*>(song)->song())) {
                        songs << static_cast<MusicLibraryItemSong*>(song)->song();
                    }
                }
            }
            break;
        case MusicLibraryItem::Type_Album:
            foreach (MusicLibraryItem *song, item->children()) {
                if (MusicLibraryItem::Type_Song==song->type() && !songs.contains(static_cast<MusicLibraryItemSong*>(song)->song())) {
                    songs << static_cast<MusicLibraryItemSong*>(song)->song();
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!songs.contains(static_cast<MusicLibraryItemSong*>(item)->song())) {
                songs << static_cast<MusicLibraryItemSong*>(item)->song();
            }
            break;
        default:
            break;
        }
    }
    return songs;
}

void DevicesModel::updateSong(const Song &orig, const Song &edit)
{
    foreach (Device *dev, devices) {
        if (Device::Ums==dev->type() && orig.file.startsWith(dev->path())) {
            dev->removeSongFromList(orig);
            dev->addSongToList(edit);
            return;
        }
    }
}

void DevicesModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (Device *dev, devices) {
        dev->getDetails(artists, albumArtists, albums, genres);
    }
}

void DevicesModel::emitAddToDevice()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToDevice(act->data().toString());
    }
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesModel::addRemoteDevice(const QString &path, const QString &coverFileName, const Device::Options &opts, const RemoteDevice::Details &details)
{
    RemoteDevice *dev=RemoteDevice::create(this, path, coverFileName, opts, details);

    if (dev) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count());
        indexes.insert(dev->udi(), devices.count());
        devices.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        connect(dev, SIGNAL(udiChanged(const QString &, const QString &)), SLOT(changeDeviceUdi(const QString &, const QString &)));
        updateItemMenu();
    }
}

void DevicesModel::removeRemoteDevice(const QString &udi)
{
    Device *dev=device(udi);

    if (dev && Device::Remote==dev->type()) {
        RemoteDevice::remove(static_cast<RemoteDevice *>(dev));
        deviceRemoved(udi);
        dev->deleteLater();
    }
}

void DevicesModel::changeDeviceUdi(const QString &from, const QString &to)
{
    if (indexes.contains(from)) {
        int idx=indexes[from];
        indexes.remove(from);
        indexes.insert(to, idx);
    }
}
#endif

void DevicesModel::updateItemMenu()
{
    if (!itemMenu) {
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();

    if (devices.isEmpty()) {
        itemMenu->addAction(i18n("No Devices Attached"))->setEnabled(false);
    } else {
        QMultiMap<QString, const Device *> items;

        foreach (const Device *d, devices) {
            items.insert(d->data(), d);
        }

        QMultiMap<QString, const Device *>::ConstIterator it(items.begin());
        QMultiMap<QString, const Device *>::ConstIterator end(items.end());

        for (; it!=end; ++it) {
            itemMenu->addAction(QIcon::fromTheme(it.value()->icon()), it.key(), this, SLOT(emitAddToDevice()))->setData(it.value()->udi());
        }
    }
}

QMimeData * DevicesModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=0;
    QStringList paths=filenames(indexes, true);

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}
