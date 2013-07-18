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
#include "dirviewmodel.h"
#include "onlineservicesmodel.h"
#include "onlineservice.h"
#include "onlinedevice.h"
#include "jamendoservice.h"
#include "magnatuneservice.h"
#include "soundcloudservice.h"
#include "playqueuemodel.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "localize.h"
#include "icons.h"
#include "filejob.h"
#include "utils.h"
#include "covers.h"
#include "stdactions.h"
#include "actioncollection.h"
#include <QStringList>
#include <QMimeData>
#include <QFile>
#include <QDir>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(OnlineServicesModel, instance)
#endif

QString OnlineServicesModel::constUdiPrefix("online-service://");

OnlineServicesModel * OnlineServicesModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static OnlineServicesModel *instance=0;
    if(!instance) {
        instance=new OnlineServicesModel;
    }
    return instance;
    #endif
}

OnlineServicesModel::OnlineServicesModel(QObject *parent)
    : MultiMusicModel(parent)
    , enabled(false)
    , dev(0)
{
    configureAction = ActionCollection::get()->createAction("configureonlineservice", i18n("Configure Online Service"), Icons::self()->configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshonlineservice", i18n("Refresh Online Service"), "view-refresh");
}

OnlineServicesModel::~OnlineServicesModel()
{
}

QModelIndex OnlineServicesModel::serviceIndex(const OnlineService *srv) const
{
    int row=collections.indexOf(const_cast<OnlineService *>(srv));
    return -1==row ? QModelIndex() : createIndex(row, 0, (void *)srv);
}

bool OnlineServicesModel::hasChildren(const QModelIndex &index) const
{
    return !index.isValid() || MusicLibraryItem::Type_Song!=static_cast<MusicLibraryItem *>(index.internalPointer())->itemType();
}

bool OnlineServicesModel::canFetchMore(const QModelIndex &index) const
{
    return index.isValid() && MusicLibraryItem::Type_Root==static_cast<MusicLibraryItem *>(index.internalPointer())->itemType() &&
            !static_cast<OnlineService *>(index.internalPointer())->isLoaded() &&
            !static_cast<OnlineService *>(index.internalPointer())->isSearchBased();
}

void OnlineServicesModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (canFetchMore(index)) {
        static_cast<OnlineService *>(index.internalPointer())->reload();
    }
}

QVariant OnlineServicesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::ToolTipRole:
        if (MusicLibraryItem::Type_Root==item->itemType() && 0!=item->childCount() &&
                static_cast<OnlineService *>(item)->isSearchBased()  && !static_cast<OnlineService *>(item)->currentSearchString().isEmpty()) {
            return MusicModel::data(index, role).toString()+"<br/>"+i18n("Last Search:%1", static_cast<OnlineService *>(item)->currentSearchString());
        }
        break;
    case ItemView::Role_SubText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);

            if (srv->isLoading()) {
                return static_cast<OnlineService *>(item)->statusMessage();
            }
            if (srv->isSearching())  {
                return i18n("Searching...");
            }
            if (!srv->isLoaded() && !srv->isSearchBased()) {
                return i18n("Not Loaded");
            }
            if (0==item->childCount() && srv->isSearchBased()) {
                return i18n("Use search to locate tracks");
            }
            if (srv->isSearchBased()) {
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Track", "%1 Tracks", item->childCount());
                #else
                return QTP_TRACKS_STR(item->childCount());
                #endif
            }
        }
        break;
    case ItemView::Role_Actions: {
        QList<Action *> actions;
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);
            if (srv->canConfigure()) {
                actions << configureAction;
            }
            if (srv->canLoad()) {
                actions << refreshAction;
            }
            if (srv->isSearchBased() || srv->isLoaded()) {
                actions << StdActions::self()->searchAction;
            }
        } else {
            actions << StdActions::self()->replacePlayQueueAction << StdActions::self()->addToPlayQueueAction;
        }

        if (!actions.isEmpty()) {
            QVariant v;
            v.setValue<QList<Action *> >(actions);
            return v;
        }
    }
    default:
        break;
    }
    return MultiMusicModel::data(index, role);
}

void OnlineServicesModel::clear()
{
    QSet<QString> names;
    foreach (MusicLibraryItemRoot *col, collections) {
        names.insert(col->id());
    }

    foreach (const QString &n, names) {
        removeService(n);
    }

    collections.clear();
}

void OnlineServicesModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    bool wasEnabled=enabled;
    enabled=e;
    if (enabled) {
        connect(Covers::self(), SIGNAL(artistImage(const Song &, const QImage &, const QString &)),
                this, SLOT(setArtistImage(const Song &, const QImage &)));
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        load();
    } else {
        if (wasEnabled) {
            stop();
        }
        clear();
    }
}

void OnlineServicesModel::stop()
{
    disconnect(Covers::self(), SIGNAL(artistImage(const Song &, const QImage &, const QString &)),
              this, SLOT(setArtistImage(const Song &, const QImage &)));
    disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
              this, SLOT(setCover(const Song &, const QImage &, const QString &)));

    foreach (MusicLibraryItemRoot *col, collections) {
        static_cast<OnlineService *>(col)->stopLoader();
    }
}

OnlineService * OnlineServicesModel::service(const QString &name)
{
    int idx=name.isEmpty() ? -1 : indexOf(name);
    return idx<0 ? 0 : static_cast<OnlineService *>(collections.at(idx));
}

void OnlineServicesModel::setArtistImage(const Song &song, const QImage &img)
{
    if (img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() || !(song.file.startsWith("http:/") || song.name.startsWith("http:/"))) {
        return;
    }

    for (int i=0; i<collections.count() ; ++i) {
        OnlineService *srv=static_cast<OnlineService *>(collections.at(i));
        if (srv->useArtistImages()) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem && artistItem->setCover(img)) {
                QModelIndex idx=index(artistItem->row(), 0, index(i, 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
    }
}

void OnlineServicesModel::setCover(const Song &song, const QImage &img, const QString &fileName)
{
    Q_UNUSED(fileName)
    if (img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() || !(song.file.startsWith("http:/") || song.name.startsWith("http:/"))) {
        return;
    }

    for (int i=0; i<collections.count() ; ++i) {
        OnlineService *srv=static_cast<OnlineService *>(collections.at(i));
        if (srv->useAlbumImages()) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem) {
                MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
                if (albumItem && albumItem->setCover(img)) {
                    QModelIndex idx=index(albumItem->row(), 0, index(artistItem->row(), 0, index(i, 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
    }
}

void OnlineServicesModel::setBusy(const QString &serviceName, bool b)
{
    int before=busyServices.count();
    if (b) {
        busyServices.insert(serviceName);
    } else {
        busyServices.remove(serviceName);
    }

    if (before!=busyServices.count()) {
        emit busy(busyServices.count());
    }
}

Qt::ItemFlags OnlineServicesModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    }
    return Qt::ItemIsEnabled;
}

OnlineService * OnlineServicesModel::addService(const QString &name)
{
    OnlineService *srv=0;

    if (!service(name)) {
        if (name==JamendoService::constName) {
            srv=new JamendoService(this);
        } else if (name==MagnatuneService::constName) {
            srv=new MagnatuneService(this);
        } else if (name==SoundCloudService::constName) {
            srv=new SoundCloudService(this);
        }

        if (srv) {
            srv->loadConfig();
            beginInsertRows(QModelIndex(), collections.count(), collections.count());
            srv->setRow(collections.count());
            collections.append(srv);
            endInsertRows();
            connect(srv, SIGNAL(error(const QString &)), SIGNAL(error(const QString &)));
        }
    }
    return srv;
}

void OnlineServicesModel::removeService(const QString &name)
{
    int idx=indexOf(name);
    if (idx<0) {
        return;
    }

    OnlineService *srv=static_cast<OnlineService *>(collections.at(idx));

    if (srv) {
        beginRemoveRows(QModelIndex(), idx, idx);
        collections.takeAt(idx);
        endRemoveRows();
        MultiMusicModel::updateGenres();
        // Destroy will stop service, and delete it (via deleteLater())
        srv->destroy();
    }
}

void OnlineServicesModel::stateChanged(const QString &name, bool state)
{
    if (!state) {
        int idx=indexOf(name);
        if (idx<0) {
            return;
        }
        MultiMusicModel::updateGenres();
    }
}

void OnlineServicesModel::load()
{
    addService(JamendoService::constName);
    addService(MagnatuneService::constName);
    addService(SoundCloudService::constName);
}

void OnlineServicesModel::setSearch(const QString &serviceName, const QString &text)
{
    OnlineService *srv=service(serviceName);

    if (srv) {
        srv->setSearch(text);
    }
}

QMimeData * OnlineServicesModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData=0;
    QStringList paths=filenames(indexes);

    if (!paths.isEmpty()) {
        mimeData=new QMimeData();
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

Device * OnlineServicesModel::device(const QString &udi)
{
    if (!dev) {
        dev=new OnlineDevice();
    }
    dev->setData(udi.mid(constUdiPrefix.length()));
    return dev;
}
