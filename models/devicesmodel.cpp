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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "devicesmodel.h"
#include "playqueuemodel.h"
#include "gui/settings.h"
#include "roles.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdconnection.h"
#include "devices/umsdevice.h"
#include "http/httpserver.h"
#include "widgets/icons.h"
#include "widgets/mirrormenu.h"
#include "devices/mountpoints.h"
#include "gui/stdactions.h"
#include "support/action.h"
#include "config.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "devices/audiocddevice.h"
#endif
#include "support/globalstatic.h"
#include <QStringList>
#include <QMimeData>
#include <QTimer>
#include "solid-lite/device.h"
#include "solid-lite/deviceinterface.h"
#include "solid-lite/devicenotifier.h"
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
#include "solid-lite/storagevolume.h"
#include "solid-lite/opticaldisc.h"
#include <algorithm>

#include <QDebug>
static bool debugIsEnabled=false;
#define DBUG if (debugIsEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void DevicesModel::enableDebug()
{
    debugIsEnabled=true;
}

bool DevicesModel::debugEnabled()
{
    return debugIsEnabled;
}

QString DevicesModel::fixDevicePath(const QString &path)
{
    // Remove MTP IDs, and display storage...
    if (path.startsWith(QChar('{')) && path.contains(QChar('}'))) {
        int end=path.indexOf(QChar('}'));
        QStringList details=path.mid(1, end-1).split(QChar('/'));
        if (details.length()>3) {
            return QChar('(')+details.at(3)+QLatin1String(") ")+path.mid(end+1);
        }
        return path.mid(end+1);
    }
    return path;
}

GLOBAL_STATIC(DevicesModel, instance)

DevicesModel::DevicesModel(QObject *parent)
    : MusicLibraryModel(parent)
    , itemMenu(nullptr)
    , enabled(false)
    , inhibitMenuUpdate(false)
{
    configureAction = new Action(Icons::self()->configureIcon, tr("Configure Device"), this);
    refreshAction = new Action(Icons::self()->reloadIcon, tr("Refresh Device"), this);
    connectAction = new Action(Icons::self()->connectIcon, tr("Connect Device"), this);
    disconnectAction = new Action(Icons::self()->disconnectIcon, tr("Disconnect Device"), this);
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    editAction = new Action(Icons::self()->editIcon, tr("Edit CD Details"), this);
    #endif
    updateItemMenu();
    connect(this, SIGNAL(add(const QStringList &, int, quint8, bool)), MPDConnection::self(), SLOT(add(const QStringList &, int, quint8, bool)));
}

DevicesModel::~DevicesModel()
{
    qDeleteAll(collections);
}

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
        return row<collections.count() ? createIndex(row, column, collections.at(row)) : QModelIndex();
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
        return createIndex(parentItem->parentItem() ? parentItem->row() : row(parentItem), 0, parentItem);
    } else {
        return QModelIndex();
    }
}

int DevicesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    return parent.isValid() ? static_cast<MusicLibraryItem *>(parent.internalPointer())->childCount() : collections.count();
}

void DevicesModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
{
    for (MusicLibraryItemRoot *col: collections) {
        col->getDetails(artists, albumArtists, composers, albums, genres);
    }
}

int DevicesModel::indexOf(const QString &id)
{
    int i=0;
    for (MusicLibraryItemRoot *col: collections) {
        if (col->id()==id) {
            return i;
        }
        i++;
    }
    return -1;
}

QList<Song> DevicesModel::songs(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QMap<MusicLibraryItem *, QList<Song> > colSongs;
    QMap<MusicLibraryItem *, QSet<QString> > colFiles;

    for (QModelIndex index: indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
        MusicLibraryItem *p=item;

        while (p->parentItem()) {
            p=p->parentItem();
        }

        if (!p) {
            continue;
        }

        MusicLibraryItemRoot *parent=static_cast<MusicLibraryItemRoot *>(p);

        if (playableOnly && !parent->canPlaySongs()) {
            continue;
        }

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            if (static_cast<MusicLibraryItemRoot *>(parent)->flat()) {
                for (const MusicLibraryItem *song: static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !colFiles[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->file())) {
                        colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                        colFiles[parent] << static_cast<const MusicLibraryItemSong*>(song)->file();
                    }
                }
            } else {
                // First, sort all artists as they would appear in UI...
                QList<MusicLibraryItem *> artists=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
                if (artists.isEmpty()) {
                    break;
                }

                std::sort(artists.begin(), artists.end(), MusicLibraryItemArtist::lessThan);

                for (MusicLibraryItem *a: artists) {
                    const MusicLibraryItemContainer *artist=static_cast<const MusicLibraryItemContainer *>(a);
                    // Now sort all albums as they would appear in UI...
                    QList<MusicLibraryItem *> artistAlbums=artist->childItems();
                    std::sort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);
                    for (MusicLibraryItem *i: artistAlbums) {
                        const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                        for (const MusicLibraryItem *song: album->childItems()) {
                            if (MusicLibraryItem::Type_Song==song->itemType() && !colFiles[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->file())) {
                                colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                                colFiles[parent] << static_cast<const MusicLibraryItemSong*>(song)->file();
                            }
                        }
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Artist: {
            // First, sort all albums as they would appear in UI...
            QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            std::sort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            for (MusicLibraryItem *i: artistAlbums) {
                const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                for (const MusicLibraryItem *song: album->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !colFiles[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->file())) {
                        colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                        colFiles[parent] << static_cast<const MusicLibraryItemSong*>(song)->file();
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album:
            for (const MusicLibraryItem *song: static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                if (MusicLibraryItem::Type_Song==song->itemType() && !colFiles[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->file())) {
                    colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song(), fullPath);
                    colFiles[parent] << static_cast<const MusicLibraryItemSong*>(song)->file();
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!colFiles[parent].contains(static_cast<const MusicLibraryItemSong*>(item)->file())) {
                colSongs[parent] << parent->fixPath(static_cast<const MusicLibraryItemSong*>(item)->song(), fullPath);
                colFiles[parent] << static_cast<const MusicLibraryItemSong*>(item)->file();
            }
            break;
        default:
            break;
        }
    }

    QList<Song> songs;
    QMap<MusicLibraryItem *, QList<Song> >::Iterator it(colSongs.begin());
    QMap<MusicLibraryItem *, QList<Song> >::Iterator end(colSongs.end());

    for (; it!=end; ++it) {
        songs.append(it.value());
    }

    return songs;
}

QStringList DevicesModel::filenames(const QModelIndexList &indexes, bool playableOnly, bool fullPath) const
{
    QList<Song> songList=songs(indexes, playableOnly, fullPath);
    QStringList fnames;
    for (const Song &s: songList) {
        fnames.append(s.file);
    }
    return fnames;
}

bool DevicesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    if (Cantata::Role_Image==role) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        if (MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::AudioCd==dev->devType()) {
                static_cast<AudioCdDevice *>(dev)->scaleCoverPix(value.toInt());
                return true;
            }
        }
    }
    #endif
   return MusicLibraryModel::setData(index, value, role);
}

QVariant DevicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (MusicLibraryItem::Type_Root==song->parentItem()->itemType()) {
                return song->song().trackAndTitleStr();
            }
        }
        break;
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    case Cantata::Role_Image:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::AudioCd==dev->devType()) {
                return static_cast<AudioCdDevice *>(dev)->coverPix();
            }
        }
        break;
    #endif
    case Cantata::Role_SubText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (!dev->statusMessage().isEmpty()) {
                return dev->statusMessage();
            }
            if (!dev->isConnected()) {
                QString sub=dev->subText();
                return tr("Not Connected")+(sub.isEmpty() ? QString() : (Song::constSep+sub));
            }
            if (Device::AudioCd==dev->devType()) {
                return dev->subText();
            }
        }
        break;
    case Cantata::Role_Capacity:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->usedCapacity();
        }
        return QVariant();
    case Cantata::Role_CapacityText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<Device *>(item)->capacityString();
        }
        return QVariant();
    case Cantata::Role_Actions: {
        QVariant v;
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            QList<Action *> actions;
            if (Device::AudioCd!=static_cast<Device *>(item)->devType()) {
                actions << configureAction;
            } else if (HttpServer::self()->isAlive()) {
                actions << StdActions::self()->replacePlayQueueAction;
            }
            actions << refreshAction;
            if (static_cast<Device *>(item)->supportsDisconnect()) {
                actions << (static_cast<Device *>(item)->isConnected() ? disconnectAction : connectAction);
            }
            #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
            if (Device::AudioCd==static_cast<Device *>(item)->devType()) {
                actions << editAction;
            }
            #endif
            v.setValue<QList<Action *> >(actions);
        } else if (root(item)->canPlaySongs() && HttpServer::self()->isAlive()) {
            v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StdActions::self()->appendToPlayQueueAction);
        }
        return v;
    }
    case Cantata::Role_ListImage:
        return MusicLibraryItem::Type_Album==item->itemType();
    default:
        break;
    }
    return MusicLibraryModel::data(index, role);
}

void DevicesModel::clear(bool clearConfig)
{
    inhibitMenuUpdate=true;
    QSet<QString> remoteUdis;
    QSet<QString> udis;
    for (MusicLibraryItemRoot *col: collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::RemoteFs==dev->devType()) {
            remoteUdis.insert(dev->id());
        } else {
            udis.insert(dev->id());
        }
    }

    for (const QString &u: udis) {
        deviceRemoved(u);
    }
    for (const QString &u: remoteUdis) {
        removeRemoteDevice(u, clearConfig);
    }

    collections.clear();
    volumes.clear();
    inhibitMenuUpdate=false;
    updateItemMenu();
}

void DevicesModel::setEnabled(bool e)
{
    StdActions::self()->copyToDeviceAction->setVisible(e);

    if (e==enabled) {
        return;
    }

    enabled=e;

    inhibitMenuUpdate=true;
    if (enabled) {
        connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
        connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        #endif
        // Call loadLocal via a timer, so that upon Cantata start-up model is loaded into view before we try and expand items!
        QTimer::singleShot(0, this, SIGNAL(loadLocal()));
        connect(MountPoints::self(), SIGNAL(updated()), this, SLOT(mountsChanged()));
        #ifdef ENABLE_REMOTE_DEVICES
        loadRemote();
        #endif
    } else {
        stop();
        clear(false);
    }
    inhibitMenuUpdate=false;
    updateItemMenu();
}

void DevicesModel::stop()
{
    for (MusicLibraryItemRoot *col: collections) {
        static_cast<Device *>(col)->stop();
    }

    disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
    disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
               this, SLOT(setCover(const Song &, const QImage &, const QString &)));
    #endif
    disconnect(MountPoints::self(), SIGNAL(updated()), this, SLOT(mountsChanged()));
    #if defined ENABLE_REMOTE_DEVICES
    unmountRemote();
    #endif
}

Device * DevicesModel::device(const QString &udi)
{
    int idx=indexOf(udi);
    return idx<0 ? nullptr : static_cast<Device *>(collections.at(idx));
}

void DevicesModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    DBUG << "Set CDDA cover" << song.file << img.isNull() << file << song.isCdda();
    if (song.isCdda()) {
        int idx=indexOf(song.title);
        if (idx>=0) {
            Device *dev=static_cast<Device *>(collections.at(idx));
            if (Device::AudioCd==dev->devType()) {
                DBUG << "Set cover of CD";
                Covers::self()->updateCover(song, img, file);
                static_cast<AudioCdDevice *>(dev)->setCover(song, img, file);
            }
        }
    }
    #else
    Q_UNUSED(song)
    Q_UNUSED(img)
    Q_UNUSED(file)
    #endif
}

void DevicesModel::setCover(const Song &song, const QImage &img)
{
    DBUG << "Set album cover" << song.file << img.isNull();
    if (img.isNull()) {
        return;
    }

    Device *dev=qobject_cast<Device *>(sender());
    if (!dev) {
        return;
    }
    int i=collections.indexOf(dev);
    if (i<0) {
        return;
    }
    MusicLibraryItemArtist *artistItem = dev->artist(song, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
        if (albumItem) {
            DBUG << "Set cover of album";
            Covers::self()->updateCover(song, img, QString());
            QModelIndex idx=index(albumItem->row(), 0, index(artistItem->row(), 0, index(i, 0, QModelIndex())));
            emit dataChanged(idx, idx);
        }
    }
}

void DevicesModel::deviceUpdating(const QString &udi, bool state)
{
    int idx=indexOf(udi);
    if (idx>=0) {
        Device *dev=static_cast<Device *>(collections.at(idx));

        if (state) {
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
        } else {
            if (dev->haveUpdate()) {
                dev->applyUpdate();
            }
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
            emit updated(modelIndex);
        }
    }
}

Qt::ItemFlags DevicesModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    }
    return Qt::NoItemFlags;
}

QStringList DevicesModel::playableUrls(const QModelIndexList &indexes) const
{
    QList<Song> songList=songs(indexes, true, true);
    QStringList urls;
    for (const Song &s: songList) {
        QByteArray encoded=HttpServer::self()->encodeUrl(s);
        if (!encoded.isEmpty()) {
            urls.append(encoded);
        }
    }
    return urls;
}

void DevicesModel::emitAddToDevice()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToDevice(act->data().toString());
    }
}

void DevicesModel::deviceAdded(const QString &udi)
{
    if (indexOf(udi)>=0) {
        return;
    }

    Solid::Device device(udi);
    DBUG << "Solid device added udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
    Solid::StorageAccess *ssa =nullptr;
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    Solid::OpticalDisc * opt = device.as<Solid::OpticalDisc>();

    if (opt && (opt->availableContent()&Solid::OpticalDisc::Audio)) {
        DBUG << "device is audiocd";
    } else
    #endif
    if ((ssa=device.as<Solid::StorageAccess>())) {
        if ((!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
            (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus())) {
            DBUG << "Found Solid::StorageAccess that is not usb, skipping";
            return;
        }
        DBUG << "volume is generic storage";
        if (!volumes.contains(device.udi())) {
            connect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
            volumes.insert(device.udi());
        }
    } else if (device.is<Solid::StorageDrive>()) {
        DBUG << "device is a Storage drive, still need a volume";
    } else if (device.is<Solid::PortableMediaPlayer>()) {
        DBUG << "device is a PMP";
    } else {
        DBUG << "device not handled";
        return;
    }
    addLocalDevice(device.udi());
}

void DevicesModel::addLocalDevice(const QString &udi)
{
    if (device(udi)) {
        return;
    }
    Device *dev=Device::create(this, udi);
    if (dev) {
        beginInsertRows(QModelIndex(), collections.count(), collections.count());
        collections.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
        connect(dev, SIGNAL(updatedDetails(QList<Song>)), SIGNAL(updatedDetails(QList<Song>)));
        connect(dev, SIGNAL(play(QList<Song>)), SLOT(play(QList<Song>)));
        connect(dev, SIGNAL(renamed()), this, SLOT(updateItemMenu()));
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        if (Device::AudioCd==dev->devType()) {
            connect(static_cast<AudioCdDevice *>(dev), SIGNAL(matches(const QString &, const QList<CdAlbum> &)),
                    SIGNAL(matches(const QString &, const QList<CdAlbum> &)));
            if (!autoplayCd.isEmpty() && static_cast<AudioCdDevice *>(dev)->isAudioDevice(autoplayCd)) {
                autoplayCd=QString();
                static_cast<AudioCdDevice *>(dev)->autoplay();
            }
        }
        #endif
        updateItemMenu();
    }
}

void DevicesModel::deviceRemoved(const QString &udi)
{
    int idx=indexOf(udi);
    DBUG << "Solid device removed udi = " << udi << idx;
    if (idx>=0) {
        if (volumes.contains(udi)) {
            Solid::Device device(udi);
            Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();
            if (ssa) {
                disconnect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
            }
            volumes.remove(udi);
        }

        beginRemoveRows(QModelIndex(), idx, idx);
        Device *dev=static_cast<Device *>(collections.takeAt(idx));
        dev->deleteLater();
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        if (Device::AudioCd==dev->devType()) {
            static_cast<AudioCdDevice *>(dev)->dequeue();
        }
        #endif
        endRemoveRows();
        updateItemMenu();
    }
}

void DevicesModel::accessibilityChanged(bool accessible, const QString &udi)
{
    Q_UNUSED(accessible)
    int idx=indexOf(udi);
    DBUG << "Solid device accesibility changed udi = " << udi << idx << accessible;
    if (idx>=0) {
        Device *dev=static_cast<Device *>(collections.at(idx));
        if (dev) {
            dev->connectionStateChanged();
            QModelIndex modelIndex=createIndex(idx, 0, dev);
            emit dataChanged(modelIndex, modelIndex);
        }
    }
}
void DevicesModel::addRemoteDevice(const DeviceOptions &opts, RemoteFsDevice::Details details)
{
    #ifdef ENABLE_REMOTE_DEVICES
    Device *dev=RemoteFsDevice::create(this, opts, details);

    if (dev) {
        beginInsertRows(QModelIndex(), collections.count(), collections.count());
        collections.append(dev);
        endInsertRows();
        connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
        connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
        if (Device::RemoteFs==dev->devType()) {
            connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged()), SLOT(remoteDeviceUdiChanged()));
        }
        updateItemMenu();
    }
    #else
    Q_UNUSED(opts)
    Q_UNUSED(details)
    #endif
}

void DevicesModel::removeRemoteDevice(const QString &udi, bool removeFromConfig)
{
    #ifdef ENABLE_REMOTE_DEVICES
    int idx=indexOf(udi);
    if (idx<0) {
        return;
    }

    Device *dev=static_cast<Device *>(collections.at(idx));

    if (dev && Device::RemoteFs==dev->devType()) {
        beginRemoveRows(QModelIndex(), idx, idx);
        // Remove device from list, but do NOT delete - it may be scanning!!!!
        collections.takeAt(idx);
        endRemoveRows();
        updateItemMenu();
        RemoteFsDevice *rfs=qobject_cast<RemoteFsDevice *>(dev);
        if (rfs) {
            // Destroy will stop device, and delete it (via deleteLater())
            rfs->destroy(removeFromConfig);
        }
    }
    #else
    Q_UNUSED(udi)
    Q_UNUSED(removeFromConfig)
    #endif
}

void DevicesModel::remoteDeviceUdiChanged()
{
    #ifdef ENABLE_REMOTE_DEVICES
    updateItemMenu();
    #endif
}

void DevicesModel::mountsChanged()
{
    #ifdef ENABLE_REMOTE_DEVICES
    for (MusicLibraryItemRoot *col: collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::RemoteFs==dev->devType() && ((RemoteFsDevice *)dev)->getDetails().isLocalFile()) {
            if (0==dev->childCount()) {
                ((RemoteFsDevice *)dev)->load();
            } else if (!dev->isConnected()) {
                ((RemoteFsDevice *)dev)->clear();
            }
        }
    }
    #endif

    // For some reason if a device without a partition (e.g. /dev/sdc) is mounted whilst cantata is running, then we receive no deviceAdded signal
    // So, as a work-around, each time a device is mounted - check for all local collections. :-)
    // BUG:127
    loadLocal();
}

void DevicesModel::loadLocal()
{
    // Build set of currently known MTP/UMS collections...
    QSet<QString> existingUdis;
    for (MusicLibraryItemRoot *col: collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::Mtp==dev->devType() || Device::Ums==dev->devType()) {
            existingUdis.insert(dev->id());
        }
    }

    QList<Solid::Device> deviceList = Solid::Device::listFromType(Solid::DeviceInterface::PortableMediaPlayer);
    for (const Solid::Device &device: deviceList) {
        if (existingUdis.contains(device.udi())) {
            existingUdis.remove(device.udi());
            continue;
        }
        if (device.as<Solid::StorageDrive>()) {
            DBUG << "Solid PMP that is also a StorageDrive, skipping, udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
            continue;
        }
        DBUG << "Solid::PortableMediaPlayer with udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
        addLocalDevice(device.udi());
    }
    deviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device: deviceList) {
        if (existingUdis.contains(device.udi())) {
            existingUdis.remove(device.udi());
            continue;
        }
        DBUG << "Solid::StorageAccess with udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
        const Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();

        if (ssa) {
            if ((!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
                (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus())) {
                DBUG << "Solid::StorageAccess that is not usb, skipping";
                continue;
            }
            if (!volumes.contains(device.udi())) {
                connect(ssa, SIGNAL(accessibilityChanged(bool, const QString&)), this, SLOT(accessibilityChanged(bool, const QString&)));
                volumes.insert(device.udi());
            }
            addLocalDevice(device.udi());
        }
    }

    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    deviceList = Solid::Device::listFromType(Solid::DeviceInterface::OpticalDisc);
    for (const Solid::Device &device: deviceList) {
        if (existingUdis.contains(device.udi())) {
            existingUdis.remove(device.udi());
            continue;
        }
        DBUG << "Solid::OpticalDisc with udi:" << device.udi() << "product:" << device.product() << "vendor:" << device.vendor();
        const Solid::OpticalDisc * opt = device.as<Solid::OpticalDisc>();
        if (opt && (opt->availableContent()&Solid::OpticalDisc::Audio)) {
            addLocalDevice(device.udi());
        } else {
            DBUG << "Solid::OpticalDisc that is not audio, skipping";
        }
    }
    #endif

    // Remove any previous MTP/UMS devices that were not listed above.
    // This is to fix BUG:127
    for (const QString &udi: existingUdis) {
        deviceRemoved(udi);
    }
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesModel::loadRemote()
{
    QList<Device *> rem=RemoteFsDevice::loadAll(this);
    if (rem.count()) {
        beginInsertRows(QModelIndex(), collections.count(), collections.count()+(rem.count()-1));
        for (Device *dev: rem) {
            collections.append(dev);
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
            if (Device::RemoteFs==dev->devType()) {
                connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged()), SLOT(remoteDeviceUdiChanged()));
            }
        }
        endInsertRows();
        updateItemMenu();
    }
}

void DevicesModel::unmountRemote()
{
    for (MusicLibraryItemRoot *col: collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::RemoteFs==dev->devType()) {
            static_cast<RemoteFsDevice *>(dev)->unmount();
        }
    }
}
#endif

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
void DevicesModel::playCd(const QString &dev)
{
    for (MusicLibraryItemRoot *col: collections) {
        Device *d=static_cast<Device *>(col);
        if (Device::AudioCd==d->devType() && static_cast<AudioCdDevice *>(d)->isAudioDevice(dev)) {
            static_cast<AudioCdDevice *>(d)->autoplay();
            return;
        }
    }
    autoplayCd=dev;
}

#endif

static bool lessThan(const QString &left, const QString &right)
{
    return left.localeAwareCompare(right)<0;
}

void DevicesModel::updateItemMenu()
{
    if (inhibitMenuUpdate) {
        return;
    }

    if (!itemMenu) {
        itemMenu = new MirrorMenu(nullptr);
    }

    itemMenu->clear();

    if (!collections.isEmpty()) {
        QMap<QString, const MusicLibraryItemRoot *> items;

        for (const MusicLibraryItemRoot *d: collections) {
            if (Device::AudioCd!=static_cast<const Device *>(d)->devType()) {
                items.insert(d->data(), d);
            }
        }

        QStringList keys=items.keys();
        std::sort(keys.begin(), keys.end(), lessThan);

        for (const QString &k: keys) {
            const MusicLibraryItemRoot *d=items[k];
            QAction *act=itemMenu->addAction(d->icon(), k, this, SLOT(emitAddToDevice()));
            act->setData(d->id());
            Action::initIcon(act);
        }
    }

    if (itemMenu->isEmpty()) {
        itemMenu->addAction(tr("No Devices Attached"))->setEnabled(false);
    }
}

QMimeData * DevicesModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=nullptr;
    QStringList paths=playableUrls(indexes);

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

void DevicesModel::play(const QList<Song> &songs)
{
    QStringList paths;
    if (HttpServer::self()->isAlive()) {
        for (const Song &s: songs) {
            paths.append(HttpServer::self()->encodeUrl(s));
        }

        if (!paths.isEmpty()) {
            emit add(paths, MPDConnection::ReplaceAndplay, 0, false);
        }
    }
}

#include "moc_devicesmodel.cpp"
