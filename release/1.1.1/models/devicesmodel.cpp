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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "devicesmodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "umsdevice.h"
#include "httpserver.h"
#include "localize.h"
#include "icons.h"
#include "mountpoints.h"
#include "stdactions.h"
#include "actioncollection.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocddevice.h"
#endif
#include <QMenu>
#include <QStringList>
#include <QMimeData>
#include <QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#include <KDE/KGlobal>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/devicenotifier.h>
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldisc.h>
K_GLOBAL_STATIC(DevicesModel, instance)
#else
#include "solid-lite/device.h"
#include "solid-lite/deviceinterface.h"
#include "solid-lite/devicenotifier.h"
#include "solid-lite/portablemediaplayer.h"
#include "solid-lite/storageaccess.h"
#include "solid-lite/storagedrive.h"
#include "solid-lite/storagevolume.h"
#include "solid-lite/opticaldisc.h"
#endif

#include <QDebug>
#define DBUG qDebug()

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
    : MultiMusicModel(parent)
    , itemMenu(0)
    , enabled(false)
    , inhibitMenuUpdate(false)
{
    configureAction = ActionCollection::get()->createAction("configuredevice", i18n("Configure Device"), Icons::self()->configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshdevice", i18n("Refresh Device"), "view-refresh");
    connectAction = ActionCollection::get()->createAction("connectdevice", i18n("Connect Device"), Icons::self()->connectIcon);
    disconnectAction = ActionCollection::get()->createAction("disconnectdevice", i18n("Disconnect Device"), Icons::self()->disconnectIcon);
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    editAction = ActionCollection::get()->createAction("editcd", i18n("Edit CD Details"), Icons::self()->editIcon);
    #endif
    updateItemMenu();
    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
}

DevicesModel::~DevicesModel()
{
}

QVariant DevicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case ItemView::Role_SubText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (!dev->statusMessage().isEmpty()) {
                return dev->statusMessage();
            }
            if (!dev->isConnected()) {
                QString sub=dev->subText();
                return i18n("Not Connected")+(sub.isEmpty() ? QString() : (QString(" - ")+sub));
            }
            if (Device::AudioCd==dev->devType()) {
                return dev->subText();
            }
        }
        break;
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
    case ItemView::Role_Actions:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            QVariant v;
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
            return v;
        } /*else if (HttpServer::self()->isAlive()) {
            MusicLibraryItem *root=static_cast<MusicLibraryItem *>(item);
            while (root && MusicLibraryItem::Type_Root!=root->itemType()) {
                root=root->parentItem();
            }
            if (root && static_cast<Device *>(root)->canPlaySongs()) {
                QVariant v;
                v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StdActions::self()->addToPlayQueueAction);
                return v;
            }
        } */
        break;
    default:
        break;
    }
    return MultiMusicModel::data(index, role);
}

void DevicesModel::clear(bool clearConfig)
{
    inhibitMenuUpdate=true;
    QSet<QString> remoteUdis;
    QSet<QString> udis;
    foreach (MusicLibraryItemRoot *col, collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::RemoteFs==dev->devType()) {
            remoteUdis.insert(dev->id());
        } else {
            udis.insert(dev->id());
        }
    }

    foreach (const QString &u, udis) {
        deviceRemoved(u);
    }
    foreach (const QString &u, remoteUdis) {
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
//        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
//                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
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
    foreach (MusicLibraryItemRoot *col, collections) {
        static_cast<Device *>(col)->stop();
    }

    disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString &)), this, SLOT(deviceAdded(const QString &)));
    disconnect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString &)), this, SLOT(deviceRemoved(const QString &)));
//        disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
//                   this, SLOT(setCover(const Song &, const QImage &, const QString &)));
    disconnect(MountPoints::self(), SIGNAL(updated()), this, SLOT(mountsChanged()));
}

Device * DevicesModel::device(const QString &udi)
{
    int idx=indexOf(udi);
    return idx<0 ? 0 : static_cast<Device *>(collections.at(idx));
}

void DevicesModel::setCover(const Song &song, const QImage &img)
{
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

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
        if (albumItem && albumItem->setCover(img)) {
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
            MultiMusicModel::updateGenres();
            emit updated(modelIndex);
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

QStringList DevicesModel::playableUrls(const QModelIndexList &indexes) const
{
    QList<Song> songList=songs(indexes, true, true);
    QStringList urls;
    foreach (const Song &s, songList) {
        urls.append(HttpServer::self()->encodeUrl(s));
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
    Solid::StorageAccess *ssa =0;
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
        connect(dev, SIGNAL(invalid(QList<Song>)), SIGNAL(invalid(QList<Song>)));
        connect(dev, SIGNAL(updatedDetails(QList<Song>)), SIGNAL(updatedDetails(QList<Song>)));
        connect(dev, SIGNAL(play(QList<Song>)), SLOT(play(QList<Song>)));
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        if (Device::AudioCd==dev->devType()) {
            connect(static_cast<AudioCdDevice *>(dev), SIGNAL(matches(const QString &, const QList<CdAlbum> &)),
                    SIGNAL(matches(const QString &, const QList<CdAlbum> &)));
            if (!autoplayCd.isEmpty() && static_cast<AudioCdDevice *>(dev)->isDevice(autoplayCd)) {
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
        static_cast<Device *>(collections.takeAt(idx))->deleteLater();
        endRemoveRows();
        updateItemMenu();
        MultiMusicModel::updateGenres();
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
            connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(connectionStateHasChanged(QString, bool)),
                    SLOT(removeDeviceConnectionStateChanged(QString, bool)));
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
        MultiMusicModel::updateGenres();
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

void DevicesModel::removeDeviceConnectionStateChanged(const QString &udi, bool state)
{
    #ifdef ENABLE_REMOTE_DEVICES
    if (!state) {
        int idx=indexOf(udi);
        if (idx<0) {
            return;
        }
        MultiMusicModel::updateGenres();
    }
    #else
    Q_UNUSED(udi)
    Q_UNUSED(state)
    #endif
}

void DevicesModel::mountsChanged()
{
    #ifdef ENABLE_REMOTE_DEVICES
    foreach (MusicLibraryItemRoot *col, collections) {
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
    foreach (MusicLibraryItemRoot *col, collections) {
        Device *dev=static_cast<Device *>(col);
        if (Device::Mtp==dev->devType() || Device::Ums==dev->devType()) {
            existingUdis.insert(dev->id());
        }
    }

    QList<Solid::Device> deviceList = Solid::Device::listFromType(Solid::DeviceInterface::PortableMediaPlayer);
    foreach (const Solid::Device &device, deviceList) {
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
    foreach (const Solid::Device &device, deviceList)
    {
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
    foreach (const Solid::Device &device, deviceList) {
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
    foreach (const QString &udi, existingUdis) {
        deviceRemoved(udi);
    }
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesModel::loadRemote()
{
    QList<Device *> rem=RemoteFsDevice::loadAll(this);
    if (rem.count()) {
        beginInsertRows(QModelIndex(), collections.count(), collections.count()+(rem.count()-1));
        foreach (Device *dev, rem) {
            collections.append(dev);
            connect(dev, SIGNAL(updating(const QString &, bool)), SLOT(deviceUpdating(const QString &, bool)));
            connect(dev, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
            connect(dev, SIGNAL(cover(const Song &, const QImage &)), SLOT(setCover(const Song &, const QImage &)));
            if (Device::RemoteFs==dev->devType()) {
                connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(udiChanged()), SLOT(remoteDeviceUdiChanged()));
                connect(static_cast<RemoteFsDevice *>(dev), SIGNAL(connectionStateHasChanged(QString, bool)),
                        SLOT(removeDeviceConnectionStateChanged(QString, bool)));
            }
        }
        endInsertRows();
        updateItemMenu();
    }
}

void DevicesModel::unmountRemote()
{
    foreach (MusicLibraryItemRoot *col, collections) {
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
    foreach (MusicLibraryItemRoot *col, collections) {
        Device *d=static_cast<Device *>(col);
        if (Device::AudioCd==d->devType() && static_cast<AudioCdDevice *>(d)->isDevice(dev)) {
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
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();

    if (collections.isEmpty()) {
        itemMenu->addAction(i18n("No Devices Attached"))->setEnabled(false);
    } else {
        QMap<QString, const MusicLibraryItemRoot *> items;

        foreach (const MusicLibraryItemRoot *d, collections) {
            if (Device::AudioCd!=static_cast<const Device *>(d)->devType()) {
                items.insert(d->data(), d);
            }
        }

        QStringList keys=items.keys();
        qSort(keys.begin(), keys.end(), lessThan);

        foreach (const QString &k, keys) {
            const MusicLibraryItemRoot *d=items[k];
            QAction *act=itemMenu->addAction(d->icon(), k, this, SLOT(emitAddToDevice()));
            act->setData(d->id());
            Action::initIcon(act);
        }
    }
}

QMimeData * DevicesModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=0;
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
        foreach (const Song &s, songs) {
            paths.append(HttpServer::self()->encodeUrl(s));
        }

        if (!paths.isEmpty()) {
            emit add(paths, true, 0);
        }
    }
}
