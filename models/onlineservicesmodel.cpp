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
    : ActionModel(parent)
    , enabled(false)
    , dev(0)
{
    configureAction = ActionCollection::get()->createAction("configureonlineservice", i18n("Configure Online Service"), Icons::self()->configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshonlineservice", i18n("Refresh Online Service"), "view-refresh");
}

OnlineServicesModel::~OnlineServicesModel()
{
    qDeleteAll(services);
}

QModelIndex OnlineServicesModel::index(int row, int column, const QModelIndex &parent) const
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
        return row<services.count() ? createIndex(row, column, services.at(row)) : QModelIndex();
    }

    return QModelIndex();
}

QModelIndex OnlineServicesModel::index(OnlineService *srv) const
{
    int row=services.indexOf(srv);
    return -1==row ? QModelIndex() : createIndex(row, 0, (void *)srv);
}

QModelIndex OnlineServicesModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    MusicLibraryItem *childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem *parentItem = childItem->parentItem();

    if (parentItem) {
        return createIndex(parentItem->parentItem() ? parentItem->row() : services.indexOf(static_cast<OnlineService *>(parentItem)), 0, parentItem);
    } else {
        return QModelIndex();
    }
}

QVariant OnlineServicesModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int OnlineServicesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? static_cast<MusicLibraryItem *>(parent.internalPointer())->childCount() : services.count();
}

int OnlineServicesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
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
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            return static_cast<OnlineService *>(item)->serviceIcon();
        }
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            return artist->isVarious() ? Icons::self()->variousArtistsIcon : Icons::self()->artistIcon;
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return Icons::self()->albumIcon;
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (MusicLibraryItem::Type_Root==song->parentItem()->itemType()) {
                return song->song().artistSong();
            } else if (static_cast<MusicLibraryItemAlbum *>(song->parentItem())->isSingleTracks()) {
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
                  (static_cast<OnlineService *>(item)->isFlat()
                        #ifdef ENABLE_KDE_SUPPORT
                        ? i18np("1 Track", "%1 Tracks", item->childCount())
                        #else
                        ? QTP_TRACKS_STR(item->childCount())
                        #endif
                        #ifdef ENABLE_KDE_SUPPORT
                        : i18np("1 Artist", "%1 Artists", item->childCount())
                        #else
                        : QTP_ARTISTS_STR(item->childCount())
                        #endif
                   )+
                  (static_cast<OnlineService *>(item)->isSearchBased() && !static_cast<OnlineService *>(item)->currentSearchString().isEmpty()
                   ? "\n"+static_cast<OnlineService *>(item)->currentSearchString()
                   : "");
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
            OnlineService *srv=static_cast<OnlineService *>(item);

            if (srv->isLoading()) {
                return i18n("Loading...");
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
            if (srv->isFlat()) {
                #ifdef ENABLE_KDE_SUPPORT
                return i18np("1 Track", "%1 Tracks", item->childCount());
                #else
                return QTP_TRACKS_STR(item->childCount());
                #endif
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
            return static_cast<OnlineService *>(item)->loadProgress();
        }
        return QVariant();
    case ItemView::Role_CapacityText:
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            return static_cast<OnlineService *>(item)->statusMessage();
        }
        return QVariant();
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
        return QVariant();
    }
    return QVariant();
}

void OnlineServicesModel::clear()
{
    QSet<QString> names;
    foreach (OnlineService *srv, services) {
        names.insert(srv->name());
    }

    foreach (const QString &n, names) {
        removeService(n);
    }

    services.clear();
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

    foreach (OnlineService *srv, services) {
        srv->stopLoader();
    }
}

OnlineService * OnlineServicesModel::service(const QString &name)
{
    int idx=name.isEmpty() ? -1 : indexOf(name);
    return idx<0 ? 0 : services.at(idx);
}

void OnlineServicesModel::setArtistImage(const Song &song, const QImage &img)
{
    if (img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() || !(song.file.startsWith("http:/") || song.name.startsWith("http:/"))) {
        return;
    }

    for (int i=0; i<services.count() ; ++i) {
        OnlineService *srv=services.at(i);
        if (srv->useArtistImages()) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem && artistItem->setCover(img)) {
                QModelIndex idx=index(srv->childItems().indexOf(artistItem), 0, index(i, 0, QModelIndex()));
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

    for (int i=0; i<services.count() ; ++i) {
        OnlineService *srv=services.at(i);
        if (srv->useAlbumImages()) {
            MusicLibraryItemArtist *artistItem = srv->artist(song, false);
            if (artistItem) {
                MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
                if (albumItem && albumItem->setCover(img)) {
                    QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(srv->childItems().indexOf(artistItem), 0, index(i, 0, QModelIndex())));
                    emit dataChanged(idx, idx);
                }
            }
        }
    }
}

void OnlineServicesModel::updateGenres()
{
    QSet<QString> newGenres;
    foreach (OnlineService *srv, services) {
        newGenres+=srv->genres();
    }
    if (newGenres!=srvGenres) {
        srvGenres=newGenres;
        emit updateGenres(srvGenres);
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

QStringList OnlineServicesModel::filenames(const QModelIndexList &indexes) const
{
    QList<Song> songList=songs(indexes);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> OnlineServicesModel::songs(const QModelIndexList &indexes) const
{
    QMap<MusicLibraryItem *, QList<Song> > srvSongs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
        MusicLibraryItem *parent=item;

        while (parent->parentItem()) {
            parent=parent->parentItem();
        }

        if (!parent) {
            continue;
        }

        OnlineService *srv=static_cast<OnlineService *>(parent);

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            if (srv->isFlat()) {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !srvSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        srvSongs[parent] << srv->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song());
                    }
                }
            } else {
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
                            if (MusicLibraryItem::Type_Song==song->itemType() && !srvSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                                srvSongs[parent] << srv->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song());
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
            qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            foreach (MusicLibraryItem *i, artistAlbums) {
                const MusicLibraryItemContainer *album=static_cast<const MusicLibraryItemContainer *>(i);
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    if (MusicLibraryItem::Type_Song==song->itemType() && !srvSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                        srvSongs[parent] << srv->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song());
                    }
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album:
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                if (MusicLibraryItem::Type_Song==song->itemType() && !srvSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
                    srvSongs[parent] << srv->fixPath(static_cast<const MusicLibraryItemSong*>(song)->song());
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!srvSongs[parent].contains(static_cast<const MusicLibraryItemSong*>(item)->song())) {
                srvSongs[parent] << srv->fixPath(static_cast<const MusicLibraryItemSong*>(item)->song());
            }
            break;
        default:
            break;
        }
    }

    QList<Song> songs;
    QMap<MusicLibraryItem *, QList<Song> >::Iterator it(srvSongs.begin());
    QMap<MusicLibraryItem *, QList<Song> >::Iterator end(srvSongs.end());

    for (; it!=end; ++it) {
        songs.append(it.value());
    }

    return songs;
}

void OnlineServicesModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (OnlineService *srv, services) {
        srv->getDetails(artists, albumArtists, albums, genres);
    }
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
            beginInsertRows(QModelIndex(), services.count(), services.count());
            services.append(srv);
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

    OnlineService *srv=services.at(idx);

    if (srv) {
        beginRemoveRows(QModelIndex(), idx, idx);
        services.takeAt(idx);
        endRemoveRows();
        updateGenres();
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
        updateGenres();
    }
}

void OnlineServicesModel::load()
{
    addService(JamendoService::constName);
    addService(MagnatuneService::constName);
    addService(SoundCloudService::constName);
}

void OnlineServicesModel::toggleGrouping()
{
    beginResetModel();
    foreach (OnlineService *srv, services) {
        srv->toggleGrouping();
    }
    endResetModel();
}

void OnlineServicesModel::setSearch(const QString &serviceName, const QString &text)
{
    OnlineService *srv=service(serviceName);

    if (srv) {
        srv->setSearch(text);
    }
}

int OnlineServicesModel::indexOf(const QString &name)
{
    int i=0;
    foreach (OnlineService *srv, services) {
        if (srv->name()==name) {
            return i;
        }
        i++;
    }
    return -1;
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
