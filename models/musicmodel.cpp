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
#ifdef ENABLE_ONLINE_SERVICES
#include "musiclibraryitempodcast.h"
#include "onlineservice.h"
#endif
#include "musicmodel.h"
#include "itemview.h"
#include "localize.h"
#include "qtplural.h"
#include "icons.h"
#include "covers.h"
#include <QStringList>

MusicModel::MusicModel(QObject *parent)
    : ActionModel(parent)
{
}

MusicModel::~MusicModel()
{
}

const MusicLibraryItemRoot * MusicModel::root(const MusicLibraryItem *item) const
{
    if (MusicLibraryItem::Type_Root==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item);
    }
    
    if (MusicLibraryItem::Type_Artist==item->itemType() || MusicLibraryItem::Type_Podcast==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item->parentItem());
    }
    
    if (MusicLibraryItem::Type_Album==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item->parentItem()->parentItem());
    }
    
    if (MusicLibraryItem::Type_Song==item->itemType()) {
        return root(item->parentItem());
    }
    
    return 0;
}

static QString parentData(const MusicLibraryItem *i)
{
    QString data;
    const MusicLibraryItem *itm=i;

    while (itm->parentItem()) {
        if (!itm->parentItem()->data().isEmpty()) {
            if (MusicLibraryItem::Type_Root==itm->parentItem()->itemType()) {
                data="<b>"+itm->parentItem()->data()+"</b><br/>"+data;
            } else {
                data=itm->parentItem()->data()+"<br/>"+data;
            }
        }
        itm=itm->parentItem();
    }

    return data;
}

QVariant MusicModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            QImage img=static_cast<MusicLibraryItemRoot *>(item)->image();
            if (!img.isNull()) {
                return img;
            }
            return static_cast<MusicLibraryItemRoot *>(item)->icon();
        }
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            return artist->isVarious() ? Icons::self()->variousArtistsIcon : Icons::self()->artistIcon;
        }
        #ifdef ENABLE_ONLINE_SERVICES
        case MusicLibraryItem::Type_Podcast:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return Icons::self()->podcastIcon;
            } else {
                return static_cast<MusicLibraryItemPodcast *>(item)->cover();
            }
        #endif
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() || !root(item)->useAlbumImages()) {
                return Icons::self()->albumIcon;
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song: return Song::Playlist==static_cast<MusicLibraryItemSong *>(item)->song().type ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (Song::Playlist==song->song().type) {
                return song->song().isCueFile() ? i18n("Cue Sheet") : i18n("Playlist");
            }
            if (MusicLibraryItem::Type_Root==song->parentItem()->itemType()) {
                return song->song().artistSong();
            }
            if (static_cast<MusicLibraryItemAlbum *>(song->parentItem())->isSingleTracks()) {
                return song->song().artistSong();
            }
            #ifdef ENABLE_ONLINE_SERVICES
            if (MusicLibraryItem::Type_Podcast==song->parentItem()->itemType()) {
                return item->data();
            }
            #endif
            return song->song().trackAndTitleStr(static_cast<MusicLibraryItemArtist *>(song->parentItem()->parentItem())->isVarious() &&
                                                 !Song::isVariousArtists(song->song().artist));
        } else if (MusicLibraryItem::Type_Album==item->itemType() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            #ifdef ENABLE_ONLINE_SERVICES
            if (MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
                return parentData(item)+data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                       Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time(), true)+
                       QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemPodcastEpisode *>(item)->published()+QLatin1String("</i></small>");
            }
            if (dynamic_cast<const OnlineService *>(root(item))) {
                return parentData(item)+data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                       Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time(), true);
            }
            #endif
            return parentData(item)+data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                   Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time(), true)+
                   QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        }
        return parentData(item)+
                (0==item->childCount()
                    ? item->data()
                    : (item->data()+"<br/>"+data(index, ItemView::Role_SubText).toString()));
    case ItemView::Role_ImageSize: {
        const MusicLibraryItemRoot *r=root(item);
        if (MusicLibraryItem::Type_Song!=item->itemType() && !MusicLibraryItemAlbum::itemSize().isNull()) { // icon/list style view...
            return MusicLibraryItemAlbum::iconSize(r->useLargeImages());
        } else if ((r->useAlbumImages() && MusicLibraryItem::Type_Album==item->itemType()) || MusicLibraryItem::Type_Podcast==item->itemType() || (r->useArtistImages() && MusicLibraryItem::Type_Artist==item->itemType())) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    }
    case ItemView::Role_SubText:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            MusicLibraryItemRoot *collection=static_cast<MusicLibraryItemRoot *>(item);

            if (collection->flat()) {
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
        }
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return QTP_ALBUMS_STR(item->childCount());
            #endif
        case MusicLibraryItem::Type_Podcast:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Episode", "%1 Episodes", item->childCount());
            #else
            return QTP_EPISODES_STR(item->childCount());
            #endif
        case MusicLibraryItem::Type_Song:
            return Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time(), true);
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track (%2)", "%1 Tracks (%2)", static_cast<MusicLibraryItemAlbum *>(item)->trackCount(),
                                                           Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #else
            return QTP_TRACKS_DURATION_STR(static_cast<MusicLibraryItemAlbum *>(item)->trackCount(),
                                          Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image: {
        QVariant v;
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            break;
        case MusicLibraryItem::Type_Artist:
            if (static_cast<MusicLibraryItemRoot *>(item->parentItem())->useArtistImages()) {
                v.setValue<QPixmap>(static_cast<MusicLibraryItemArtist *>(item)->cover());
            }
            break;
        #ifdef ENABLE_ONLINE_SERVICES
        case MusicLibraryItem::Type_Podcast:
            v.setValue<QPixmap>(static_cast<MusicLibraryItemPodcast *>(item)->cover());
        #endif
        default:
            break;
        }
        return v;
    }
    case ItemView::Role_TitleText:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return i18nc("Album by Artist", "%1 by %2", item->data(), item->parentItem()->data());
        }
        return item->data();
    case Qt::SizeHintRole: {
        const MusicLibraryItemRoot *r=root(item);
        if (!r->useArtistImages() && MusicLibraryItem::Type_Artist==item->itemType()) {
            return QVariant();
        }
        if (r->useLargeImages() && MusicLibraryItem::Type_Song!=item->itemType() && !MusicLibraryItemAlbum::itemSize().isNull()) {
            return MusicLibraryItemAlbum::itemSize();
        }
    }
    default:
        break;
    }
    return ActionModel::data(index, role);
}
