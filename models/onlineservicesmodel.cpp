/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibraryitempodcast.h"
#include "musiclibrarymodel.h"
#include "dirviewmodel.h"
#include "onlineservicesmodel.h"
#include "online/onlineservice.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "online/onlinedevice.h"
#endif
#include "online/jamendoservice.h"
#include "online/magnatuneservice.h"
#include "online/soundcloudservice.h"
#include "online/podcastservice.h"
#include "playqueuemodel.h"
#include "roles.h"
#include "mpd/mpdparseutils.h"
#include "support/localize.h"
#include "gui/plurals.h"
#include "widgets/icons.h"
#include "devices/filejob.h"
#include "support/utils.h"
#include "gui/stdactions.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#ifdef ENABLE_STREAMS
#include "streamsmodel.h"
#endif
#include <QStringList>
#include <QMimeData>
#include <QFile>
#include <QDir>
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

QString OnlineServicesModel::constUdiPrefix("online-service://");

GLOBAL_STATIC(OnlineServicesModel, instance)

OnlineServicesModel::OnlineServicesModel(QObject *parent)
    : MultiMusicModel(parent)
    , enabled(false)
    , dev(0)
    , podcast(0)
{
    configureAction = ActionCollection::get()->createAction("configureonlineservice", i18n("Configure Service"), Icons::self()->configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshonlineservice", i18n("Refresh Service"), "view-refresh");
    subscribeAction = ActionCollection::get()->createAction("subscribeonlineservice", i18n("Add Subscription"), "list-add");
    unSubscribeAction = ActionCollection::get()->createAction("unsubscribeonlineservice", i18n("Remove Subscription"), "list-remove");
    refreshSubscriptionAction = ActionCollection::get()->createAction("refreshsubscription", i18n("Refresh Subscription"), "view-refresh");
    #ifdef ENABLE_STREAMS
    searchAction = StreamsModel::self()->searchAct();
    #else
    // For Mac/Unity we try to hide icons from menubar menus. However, search is used in the menubar AND in the streams view. We
    // need the icon on the streams view. Therefore, if the StdAction has no icon -  we create a new one and forward all signals...
    if (StdActions::self()->searchAction->icon().isNull()) {
        searchAction = new Action(Icon("edit-find"), StdActions::self()->searchAction->text(), this);
        searchAction->setToolTip(StdActions::self()->searchAction->toolTip());
        connect(searchAction, SIGNAL(triggered(bool)), StdActions::self()->searchAction, SIGNAL(triggered(bool)));
        connect(searchAction, SIGNAL(triggered()), StdActions::self()->searchAction, SIGNAL(triggered()));
        connect(ActionCollection::get(), SIGNAL(tooltipUpdated(QAction *)), SLOT(tooltipUpdated(QAction *)));
    } else {
        searchAction = StdActions::self()->searchAction;
    }
    #endif
    load();
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
}

OnlineServicesModel::~OnlineServicesModel()
{
}

QModelIndex OnlineServicesModel::serviceIndex(const OnlineService *srv) const
{
    int r=row(const_cast<OnlineService *>(srv));
    return -1==r ? QModelIndex() : createIndex(r, 0, (void *)srv);
}

bool OnlineServicesModel::hasChildren(const QModelIndex &index) const
{
    return !index.isValid() || MusicLibraryItem::Type_Song!=static_cast<MusicLibraryItem *>(index.internalPointer())->itemType();
}

bool OnlineServicesModel::canFetchMore(const QModelIndex &index) const
{
    return index.isValid() && MusicLibraryItem::Type_Root==static_cast<MusicLibraryItem *>(index.internalPointer())->itemType() &&
            !static_cast<OnlineService *>(index.internalPointer())->isLoaded() &&
            !static_cast<OnlineService *>(index.internalPointer())->isSearchBased() &&
            !static_cast<OnlineService *>(index.internalPointer())->isPodcasts();
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
    case Cantata::Role_SubText:
        switch(item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            OnlineService *srv=static_cast<OnlineService *>(item);

            if (srv->isLoading()) {
                return static_cast<OnlineService *>(item)->statusMessage();
            }
            if (srv->isSearching())  {
                return i18n("Searching...");
            }
            if (!srv->isLoaded() && !srv->isSearchBased() && !srv->isPodcasts()) {
                return i18n("Not Loaded");
            }
            if (0==item->childCount() && srv->isSearchBased()) {
                return i18n("Use search to locate tracks");
            }
            if (srv->isSearchBased()) {
                return Plurals::tracks(item->childCount());
            }
            if (srv->isPodcasts()) {
                return Plurals::podcasts(item->childCount());
            }
            break;
        }
        case MusicLibraryItem::Type_Song: {
            if (MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
                if (static_cast<MusicLibraryItemPodcastEpisode *>(item)->downloadProgress()>=0) {
                    return MultiMusicModel::data(index, role).toString()+QLatin1Char(' ')+
                           i18n("(Downloading: %1%)", static_cast<MusicLibraryItemPodcastEpisode *>(item)->downloadProgress());
                }
            }
            break;
        }
        default:
            break;
        }
        break;
    case Cantata::Role_Actions: {
        QList<Action *> actions;
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);
            if (srv->canConfigure()) {
                actions << configureAction;
            }
            if (srv->canLoad()) {
                actions << refreshAction;
            } else if (srv->canSubscribe() && !srv->childItems().isEmpty()) {
                actions << refreshSubscriptionAction;
            }
            if (srv->isSearchBased() || srv->isLoaded()) {
                actions << searchAction;
            }
            if (srv->canSubscribe()) {
                actions << subscribeAction;
            }
        } else if (MusicLibraryItem::Type_Podcast==item->itemType()) {
            actions << refreshSubscriptionAction;
        } else {
            actions << StdActions::self()->replacePlayQueueAction << StdActions::self()->addToPlayQueueAction;
        }

        if (!actions.isEmpty()) {
            QVariant v;
            v.setValue<QList<Action *> >(actions);
            return v;
        }
        break;
    }
    case Cantata::Role_ListImage:
        return MusicLibraryItem::Type_Album==item->itemType() || MusicLibraryItem::Type_Podcast==item->itemType();
    case Qt::DecorationRole:
        if (MusicLibraryItem::Type_Song==item->itemType() && item->parentItem() && MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
            if (!static_cast<MusicLibraryItemPodcastEpisode *>(item)->localPath().isEmpty()) {
                return Icons::self()->downloadedPodcastEpisodeIcon;
            }
        }
        break;
    case Qt::FontRole:
        if ((MusicLibraryItem::Type_Song==item->itemType() && item->parentItem() && MusicLibraryItem::Type_Podcast==item->parentItem()->itemType() &&
             !static_cast<MusicLibraryItemSong *>(item)->song().hasBeenPlayed()) ||
            (MusicLibraryItem::Type_Podcast==item->itemType() && static_cast<MusicLibraryItemPodcast *>(item)->unplayedEpisodes()>0)) {
            QFont f;
            f.setBold(true);
            return f;
        }
        break;
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Podcast==item->itemType() && static_cast<MusicLibraryItemPodcast *>(item)->unplayedEpisodes()) {
            return i18nc("podcast name (num unplayed episodes)", "%1 (%2)", item->data(), static_cast<MusicLibraryItemPodcast *>(item)->unplayedEpisodes());
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

    if (e) {
        connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    } else {
        disconnect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    }

    enabled=e;
    if (!enabled) {
        stop();
    }
}

void OnlineServicesModel::stop()
{
    foreach (MusicLibraryItemRoot *col, collections) {
        static_cast<OnlineService *>(col)->stopLoader();
    }
}

OnlineService * OnlineServicesModel::service(const QString &name)
{
    int idx=name.isEmpty() ? -1 : indexOf(name);
    if (idx>=0) {
        return static_cast<OnlineService *>(collections.at(idx));
    }
    foreach (OnlineService *srv, hiddenServices) {
         if (srv->id()==name) {
             return srv;
         }
    }
    return 0;
}

void OnlineServicesModel::setBusy(const QString &id, bool b)
{
    int before=busyServices.count();
    if (b) {
        busyServices.insert(id);
    } else {
        busyServices.remove(id);
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
    return Qt::NoItemFlags;
}

OnlineService * OnlineServicesModel::addService(const QString &name, const QSet<QString> &hidden)
{
    OnlineService *srv=0;

    if (!service(name)) {
        if (name==JamendoService::constName) {
            srv=new JamendoService(this);
        } else if (name==MagnatuneService::constName) {
            srv=new MagnatuneService(this);
        } else if (name==SoundCloudService::constName) {
            srv=new SoundCloudService(this);
        } else if (name==PodcastService::constName) {
            srv=podcast=new PodcastService(this);
        }

        if (srv) {
            srv->loadConfig();
            if (hidden.contains(srv->id())) {
                hiddenServices.append(srv);
            } else {
                beginInsertRows(QModelIndex(), collections.count(), collections.count());
                collections.append(srv);
                endInsertRows();
            }
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
    QSet<QString> hidden=Settings::self()->hiddenOnlineProviders().toSet();
    addService(JamendoService::constName, hidden);
    addService(MagnatuneService::constName, hidden);
    addService(SoundCloudService::constName, hidden);
    addService(PodcastService::constName, hidden);
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

#ifdef ENABLE_DEVICES_SUPPORT
Device * OnlineServicesModel::device(const QString &udi)
{
    if (!dev) {
        dev=new OnlineDevice();
    }
    dev->setData(udi.mid(constUdiPrefix.length()));
    return dev;
}
#endif

bool OnlineServicesModel::subscribePodcast(const QUrl &url)
{
    if (podcast && !podcast->subscribedToUrl(url)) {
        podcast->addUrl(url);
        return true;
    }
    return false;
}

void OnlineServicesModel::downloadPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes)
{
    podcast->downloadPodcasts(pod, episodes);
}

void OnlineServicesModel::deleteDownloadedPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes)
{
    podcast->deleteDownloadedPodcasts(pod, episodes);
}

bool OnlineServicesModel::isDownloading() const
{
    return podcast->isDownloading();
}

void OnlineServicesModel::cancelAll()
{
    podcast->cancelAllJobs();
}

// Required due to icon missing for StdActions::searchAction for Mac/Unity... See note in constructor above.
void OnlineServicesModel::tooltipUpdated(QAction *act)
{
    #ifdef ENABLE_STREAMS
    Q_UNUSED(act)
    #else
    if (act!=searchAction && act==StdActions::self()->searchAction) {
        searchAction->setToolTip(StdActions::self()->searchAction->toolTip());
    }
    #endif
}

void OnlineServicesModel::coverLoaded(const Song &song, int size)
{
    Q_UNUSED(size)
    if (song.isArtistImageRequest() || !song.isFromOnlineService()) {
        return;
    }

    QString id=song.onlineService();
    OnlineService *srv=service(id);

    if (srv) {
        if (PodcastService::constName==id) {
            MusicLibraryItemPodcast *pod = static_cast<PodcastService *>(srv)->getPodcast(QUrl(song.file));
            if (pod) {
                QModelIndex idx=index(pod->row(), 0, index(srv->row(), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        } else {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem) {
                MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
                if (albumItem) {
                    QModelIndex idx=index(albumItem->row(), 0, index(artistItem->row(), 0, QModelIndex()));
                    emit dataChanged(idx, idx);
                }
            }
        }
    }
}

void OnlineServicesModel::save()
{
    QStringList disabled;
    foreach (OnlineService *i, hiddenServices) {
        disabled.append(i->id());
    }
    disabled.sort();
    Settings::self()->saveHiddenOnlineProviders(disabled);
}

QList<OnlineServicesModel::Provider> OnlineServicesModel::getProviders() const
{
    QList<Provider> providers;
    foreach (OnlineService *i, hiddenServices) {
        providers.append(Provider(i->data(), static_cast<OnlineService *>(i)->icon(), static_cast<OnlineService *>(i)->id(), true,
                                  static_cast<OnlineService *>(i)->canConfigure()));
    }
    foreach (MusicLibraryItemRoot *i, collections) {
        providers.append(Provider(i->data(), static_cast<OnlineService *>(i)->icon(), static_cast<OnlineService *>(i)->id(), false,
                                  static_cast<OnlineService *>(i)->canConfigure()));
    }
    return providers;
}

void OnlineServicesModel::setHiddenProviders(const QSet<QString> &prov)
{
//    bool added=false;
    bool changed=false;
    foreach (OnlineService *i, hiddenServices) {
        if (!prov.contains(i->id())) {
            beginInsertRows(QModelIndex(), collections.count(), collections.count());
            collections.append(i);
            hiddenServices.removeAll(i);
            endInsertRows();
            /*added=*/changed=true;
        }
    }

    foreach (MusicLibraryItemRoot *i, collections) {
        if (prov.contains(static_cast<OnlineService *>(i)->id())) {
            int r=row(i);
            if (r>=0) {
                beginRemoveRows(QModelIndex(), r, r);
                hiddenServices.append(static_cast<OnlineService *>(collections.takeAt(r)));
                endRemoveRows();
                changed=true;
            }
        }
    }
//    if (added && collections.count()>1) {
//        emit needToSort();
//    }
    updateGenres();
    if (changed) {
        emit providersChanged();
    }
}
