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
#include "musiclibrarymodel.h"
#include "devicesmodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "covers.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "mediadevicecache.h"
#include "umsdevice.h"
#include "httpserver.h"
#include "localize.h"
#include "icon.h"
#include <QtGui/QMenu>
#include <QtCore/QStringList>
#include <QtCore/QMimeData>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
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
    static DevicesModel *instance=0;
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
    QList<Device *> rem=RemoteFsDevice::loadAll(this);
    if (rem.count()) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count()+(rem.count()-1));
        foreach (Device *dev, rem) {
            indexes.insert(dev->udi(), devices.count());
            devices.append(dev);
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            if (Device::RemoteFs==dev->devType()) {
                connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged(const QString &, const QString &)), SLOT(changeDeviceUdi(const QString &, const QString &)));
            }
        }
        endInsertRows();
        updateItemMenu();
    }
}

void DevicesModel::unmountRemote()
{
    foreach (Device *dev, devices) {
        if (Device::RemoteFs==dev->devType()) {
            static_cast<RemoteFsDevice *>(dev)->unmount();
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
            return row<p->childCount() ? createIndex(row, column, p->childItem(row)) : QModelIndex();
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
    MusicLibraryItem *parentItem = childItem->parentItem();

    if (parentItem) {
        return createIndex(parentItem->parentItem() ? parentItem->row() : devices.indexOf(static_cast<Device *>(parentItem)), 0, parentItem);
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
        switch (item->itemType()) {
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
                return QIcon::fromTheme(DEFAULT_ALBUM_ICON);
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (static_cast<MusicLibraryItemAlbum *>(song->parentItem())->isSingleTracks()) {
                return song->song().artistSong();
            } else {
                return song->song().trackAndTitleStr(static_cast<MusicLibraryItemArtist *>(song->parentItem()->parentItem())->isVarious() &&
                                                     !Song::isVariousArtists(song->song().artist));
            }
        } else if(MusicLibraryItem::Type_Album==item->itemType() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Artist", "%1 Artists", item->childCount());
                    #else
                    QTP_ARTISTS_STR(item->childCount());
                    #endif
        case MusicLibraryItem::Type_Artist:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Album", "%1 Albums", item->childCount());
                    #else
                    QTP_ALBUMS_STR(item->childCount());
                    #endif
        case MusicLibraryItem::Type_Album:
            return 0==item->childCount()
                ? item->data()
                : item->data()+"\n"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Track (%2)", "%1 Tracks (%2)",item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
                    #else
                    QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
                    #endif
        case MusicLibraryItem::Type_Song:
            return data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time())+
                   QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->itemType()) {
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
            return QTP_ARTISTS_STR(item->childCount());
            #endif
            break;
        }
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return QTP_ALBUMS_STR(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song:
            return Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time());
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track (%2)", "%1 Tracks (%2)", item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #else
            return QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        }
        return QVariant();
    case ItemView::Role_Capacity:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->usedCapacity();
        }
        return QVariant();
    case ItemView::Role_CapacityText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->capacityString();
        }
        return QVariant();
    case ItemView::Role_ToggleIcon:
        #ifdef ENABLE_REMOTE_DEVICES
        if (MusicLibraryItem::Type_Root==item->itemType() && Device::RemoteFs==static_cast<Device *>(item)->devType() &&
            static_cast<Device *>(item)->supportsDisconnect()) {
            return static_cast<Device *>(item)->isConnected() ? Icon::connectIcon : Icon::disconnectIcon;
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
//     emit updateGenres(QSet<QString>());
}

void DevicesModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    enabled=e;

    if (enabled) {
        connect(MediaDeviceCache::self(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
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
        disconnect(Covers::self(), SIGNAL(cover(cconst Song &, const QImage &, const QString &)),
                   this, SLOT(setCover(const Song &, const QImage &, const QString &)));
    }
}

Device * DevicesModel::device(const QString &udi)
{
    if (indexes.contains(udi)) {
        return devices.at(indexes[udi]);
    }
    return 0;
}

void DevicesModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    if (img.isNull()) {
        return;
    }

    int i=0;
    foreach (MusicLibraryItem *device, devices) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemRoot *>(device)->artist(song, false);
        if (artistItem) {
            MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
            if (albumItem) {
                if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
                    QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(static_cast<MusicLibraryItemContainer *>(device)->childItems().indexOf(artistItem), 0, index(i, 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
        i++;
    }

//     foreach (const MusicLibraryItem *device, devices) {
//         bool found=false;
//         int j=0;
//         foreach (const MusicLibraryItem *artistItem, device->childItems()) {
//             if (artistItem->data()==artist) {
//                 int k=0;
//                 foreach (const MusicLibraryItem *albumItem, artistItem->childItems()) {
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

QStringList DevicesModel::filenames(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QList<Song> songList=songs(indexes, playableOnly, fullPath);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

static Song fixPath(Song s, const QString &path)
{
    if (!path.isEmpty()) {
        s.file=path+s.file;
    }
    return s;
}

QList<Song> DevicesModel::songs(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QMap<MusicLibraryItem *, QList<Song> > devSongs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
        MusicLibraryItem *parent=item;

        while (parent->parentItem()) {
            parent=parent->parentItem();
        }

        if (!parent) {
            continue;
        }
        if (playableOnly && !static_cast<Device *>(parent)->canPlaySongs()) {
            continue;
        }

        QString base=fullPath ? static_cast<Device *>(parent)->path() : QString();

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            // First, sort all artists as they would appear in UI...
            QList<MusicLibraryItem *> artists=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artists.begin(), artists.end(), MusicLibraryItemArtist::lessThan);
            foreach (MusicLibraryItem *a, artists) {
                const MusicLibraryItemContainer *artist=static_cast<const MusicLibraryItemContainer *>(a);
                // Now sort all albums as they would appear in UI...
                QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(artist)->childItems();
                qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);
                foreach (MusicLibraryItem *i, artistAlbums) {
                    const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                    foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                        if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                            devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                        }
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Artist: {
            // First, sort all albums as they would appear in UI...
            QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            foreach (MusicLibraryItem *i, artistAlbums) {
                const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album:
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                if (MusicLibraryItem::Type_Song==song->itemType() && !devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                    devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), base);
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!devSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(item)->song())) {
                devSongs[parent] << fixPath(static_cast<const MusicLibraryItemSong*>(item)->song(), base);
            }
            break;
        default:
            break;
        }
    }

    QList<Song> songs;
    QMap<MusicLibraryItem *, QList<Song> >::Iterator it(devSongs.begin());
    QMap<MusicLibraryItem *, QList<Song> >::Iterator end(devSongs.end());

    for (; it!=end; ++it) {
        songs.append(it.value());
    }

    return songs;
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
void DevicesModel::addRemoteDevice(const QString &coverFileName, const DeviceOptions &opts, RemoteFsDevice::Details details)
{
    Device *dev=RemoteFsDevice::create(this, coverFileName, opts, details);

    if (dev) {
        beginInsertRows(QModelIndex(), devices.count(), devices.count());
        indexes.insert(dev->udi(), devices.count());
        devices.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        if (Device::RemoteFs==dev->devType()) {
            connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged(const QString &, const QString &)), SLOT(changeDeviceUdi(const QString &, const QString &)));
        }
        updateItemMenu();
    }
}

void DevicesModel::removeRemoteDevice(const QString &udi)
{
    Device *dev=device(udi);

    if (dev && Device::RemoteFs==dev->devType()) {
        int idx=indexes[udi];
        beginRemoveRows(QModelIndex(), idx, idx);
        // Remove device from list, but do NOT delete - it may be scanning!!!!
        devices.takeAt(idx);
        indexes.remove(udi);
        endRemoveRows();
        updateItemMenu();
        // Remove will stop device, and delete it
        RemoteFsDevice::remove(dev);
    }
}

void DevicesModel::changeDeviceUdi(const QString &from, const QString &to)
{
    if (indexes.contains(from)) {
        int idx=indexes[from];
        indexes.remove(from);
        indexes.insert(to, idx);
        updateItemMenu();
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
    QStringList paths;
    if (HttpServer::self()->isAlive()) {
        QList<Song> songList=songs(indexes, true, true);
        foreach (const Song &s, songList) {
            paths.append(HttpServer::self()->encodeUrl(s));
        }
    } else {
        paths=filenames(indexes, true, true);
    }

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}
