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
#include "onlineservice.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "onlinedevice.h"
#endif
#include "jamendoservice.h"
#include "magnatuneservice.h"
#include "soundcloudservice.h"
#include "podcastservice.h"
#include "playqueuemodel.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "localize.h"
#include "qtplural.h"
#include "icons.h"
#include "filejob.h"
#include "utils.h"
#include "stdactions.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include <QStringList>
#include <QMimeData>
#include <QFile>
#include <QDir>
#include <QDebug>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(OnlineServicesModel, instance)
#endif

#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
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
        #if defined ENABLE_MODEL_TEST
        new ModelTest(instance, instance);
        #endif
    }
    return instance;
    #endif
}

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
    load();
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
    case ItemView::Role_SubText:
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
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Track", "%1 Tracks", item->childCount());
                #else
                return QTP_TRACKS_STR(item->childCount());
                #endif
            }
            if (srv->isPodcasts()) {
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Podcast", "%1 Podcasts", item->childCount());
                #else
                return QTP_PODCASTS_STR(item->childCount());
                #endif
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
    case ItemView::Role_Actions: {
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
                actions << StdActions::self()->searchAction;
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

void OnlineServicesModel::resetModel()
{
    OnlineService *srv=service(PodcastService::constName);
    if (srv) {
        foreach (MusicLibraryItem *p, srv->childItems()) {
            if (MusicLibraryItem::Type_Podcast==p->itemType()) {
                MusicLibraryItemPodcast *pc=static_cast<MusicLibraryItemPodcast *>(p);
                pc->clearImage();
            }
        }
    }
    ActionModel::resetModel();
}

static const char * constExtensions[]={".jpg", ".png", 0};
static const char * constIdProperty="id";
static const char * constArtistProperty="artist";
static const char * constAlbumProperty="album";
static const char * constCacheProperty="cacheName";
static const char * constMaxSizeProperty="maxSize";

#define DBUG if (Covers::debugEnabled()) qWarning() << "OnlineServicesModel" << __FUNCTION__

static Covers::Image readCache(const QString &id, const QString &artist, const QString &album)
{
    DBUG << id << artist << album;
    Covers::Image i;
    QString baseName=Utils::cacheDir(id.toLower(), false)+Covers::encodeName(album.isEmpty() ? artist : (artist+" - "+album));
    for (int e=0; constExtensions[e]; ++e) {
        QString fileName=baseName+constExtensions[e];
        if (QFile::exists(fileName)) {
            QImage img(fileName);

            if (!img.isNull()) {
                DBUG << "Read from cache" << fileName;
                i.img=img;
                i.fileName=fileName;
                break;
            }
        }
    }
    return i;
}

Covers::Image OnlineServicesModel::readImage(const Song &song)
{
    DBUG << song.file << song.albumArtist() << song.album;
    QUrl u(song.file);
    Covers::Image img;
    QString id;
    if (u.host().contains(JamendoService::constName, Qt::CaseInsensitive)) {
        id=JamendoService::constName;
    } else if (u.host().contains(MagnatuneService::constName, Qt::CaseInsensitive)) {
        id=MagnatuneService::constName;
    }

    if (!id.isEmpty()) {
        img=readCache(id, song.albumArtist(), song.album);
    }

    return img;
}

QImage OnlineServicesModel::requestImage(const QString &id, const QString &artist, const QString &album, const QString &url,
                                         const QString cacheName, int maxSize)
{
    DBUG << id << artist << album << url << cacheName << maxSize;
    if (cacheName.isEmpty()) {
        Covers::Image i=readCache(id, artist, album);
        if (!i.img.isNull()) {
            return i.img;
        }
    } else if (QFile::exists(cacheName)) {
        QImage img(cacheName);
        if (!img.isNull()) {
            DBUG << "Used cache filename";
            return img;
        }
    }

    QString imageUrl=url;
    // Jamendo image URL is just the album ID!
    if (!imageUrl.isEmpty() && !imageUrl.startsWith("http:/") && imageUrl.length()<15 && id==JamendoService::constName) {
        imageUrl=JamendoService::imageUrl(imageUrl);
        DBUG << "Built jamendo url" << imageUrl;
    }

    if (!imageUrl.isEmpty()) {
        DBUG << "Download url";
        // Need to download image...
        NetworkJob *j=NetworkAccessManager::self()->get(QUrl(imageUrl));
        j->setProperty(constIdProperty, id);
        j->setProperty(constArtistProperty, artist);
        j->setProperty(constAlbumProperty, album);
        j->setProperty(constCacheProperty, cacheName);
        j->setProperty(constMaxSizeProperty, maxSize);

        connect(j, SIGNAL(finished()), this, SLOT(imageDownloaded()));
    }

    return QImage();
}

void OnlineServicesModel::imageDownloaded()
{
    NetworkJob *j=qobject_cast<NetworkJob *>(sender());

    if (!j) {
        return;
    }

    j->deleteLater();
    QByteArray data=QNetworkReply::NoError==j->error() ? j->readAll() : QByteArray();
    if (data.isEmpty()) {
        DBUG << j->url().toString() << "empty!";
        return;
    }

    QString url=j->url().toString();
    QImage img=QImage::fromData(data, Covers::imageFormat(data));
    if (img.isNull()) {
        DBUG << url << "null image";
        return;
    }

    bool png=Covers::isPng(data);
    QString id=j->property(constIdProperty).toString();
    Song song;
    song.albumartist=song.artist=j->property(constArtistProperty).toString();
    song.album=j->property(constAlbumProperty).toString();
    DBUG << "Got image" << id << song.artist << song.album << png;
    OnlineService *srv=service(id);
    if (id==PodcastService::constName) {
        MusicLibraryItem *podcast=srv->childItem(song.artist);
        if (podcast && static_cast<MusicLibraryItemPodcast *>(podcast)->setCover(img)) {
            QModelIndex idx=index(podcast->row(), 0, index(row(srv), 0, QModelIndex()));
            emit dataChanged(idx, idx);

        }
    } else if (song.album.isEmpty()) {
        if (srv->useArtistImages() && srv->id()==id) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem && artistItem->setCover(img)) {
                QModelIndex idx=index(artistItem->row(), 0, index(row(srv), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
    } else {
        if (srv->useAlbumImages() && !srv->isPodcasts() && srv->id()==id) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem) {
                MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
                if (albumItem && albumItem->setCover(img)) {
                    QModelIndex idx=index(albumItem->row(), 0, index(artistItem->row(), 0, index(row(srv), 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
    }

    int maxSize=j->property(constMaxSizeProperty).toInt();
    QString cacheName=j->property(constCacheProperty).toString();
    QString fileName=(cacheName.isEmpty()
                        ? Utils::cacheDir(id.toLower(), true)+Covers::encodeName(song.album.isEmpty() ? song.artist : (song.artist+" - "+song.album))+(png ? ".png" : ".jpg")
                        : cacheName);

    if (!img.isNull() && (maxSize>0 || !cacheName.isEmpty())) {
        if (maxSize>32 && (img.width()>maxSize || img.height()>maxSize)) {
            img=img.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        DBUG << "Saved scaled image to" << fileName << maxSize;
        img.save(fileName);
        if (!song.album.isEmpty()) {
            song.track=1;
            //song.setKey();
            Covers::self()->emitCoverUpdated(song, img, fileName);
        }
        return;
    }
    QFile f(fileName);
    if (f.open(QIODevice::WriteOnly)) {
        DBUG << "Saved image to" << fileName;
        f.write(data);
        if (!song.album.isEmpty()) {
            song.track=1;
            //song.setKey();
            Covers::self()->emitCoverUpdated(song, img, fileName);
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
