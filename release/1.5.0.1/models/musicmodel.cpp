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
#include "online/onlineservice.h"
#endif
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#endif
#include "musicmodel.h"
#include "roles.h"
#include "support/localize.h"
#include "gui/plurals.h"
#include "gui/settings.h"
#include "widgets/icons.h"
#include "gui/covers.h"
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
    #ifndef ENABLE_UBUNTU
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root:
            return static_cast<MusicLibraryItemRoot *>(item)->icon();
        case MusicLibraryItem::Type_Artist:
            return static_cast<MusicLibraryItemArtist *>(item)->isVarious() ? Icons::self()->variousArtistsIcon : Icons::self()->artistIcon;
        #ifdef ENABLE_ONLINE_SERVICES
        case MusicLibraryItem::Type_Podcast:
            return Icons::self()->podcastIcon;
        #endif
        case MusicLibraryItem::Type_Album:
            return Icons::self()->albumIcon;
        case MusicLibraryItem::Type_Song:
            return Song::Playlist==static_cast<MusicLibraryItemSong *>(item)->song().type ? Icons::self()->playlistIcon : Icons::self()->audioFileIcon;
        default:
            return QVariant();
        }
    #endif
    case Cantata::Role_BriefMainText:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return item->data();
        }
    case Cantata::Role_MainText:
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
            return song->song().trackAndTitleStr();
        }
        return item->displayData();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            #ifdef ENABLE_ONLINE_SERVICES
            if (MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
                return parentData(item)+data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                       Utils::formatTime(static_cast<MusicLibraryItemSong *>(item)->time(), true)+
                       QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemPodcastEpisode *>(item)->published()+QLatin1String("</i></small>");
            }
            #endif
            return static_cast<MusicLibraryItemSong *>(item)->song().toolTip();
        }

        return parentData(item)+
                (0==item->childCount()
                    ? item->displayData(true)
                    : (item->displayData(true)+"<br/>"+data(index, Cantata::Role_SubText).toString()));
    case Cantata::Role_SubText:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root: {
            MusicLibraryItemRoot *collection=static_cast<MusicLibraryItemRoot *>(item);

            if (collection->flat()) {
                return Plurals::tracks(item->childCount());
            }
            return Plurals::artists(item->childCount());
        }
        case MusicLibraryItem::Type_Artist:
            return Plurals::albums(item->childCount());
        case MusicLibraryItem::Type_Podcast:
            return Plurals::episodes(item->childCount());
        case MusicLibraryItem::Type_Song:
            return Utils::formatTime(static_cast<MusicLibraryItemSong *>(item)->time(), true);
        case MusicLibraryItem::Type_Album:
            return Plurals::tracksWithDuration(static_cast<MusicLibraryItemAlbum *>(item)->trackCount(),
                                               Utils::formatTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
        default: return QVariant();
        }
    case Cantata::Role_ListImage:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            return true;
        #ifdef ENABLE_ONLINE_SERVICES
        case MusicLibraryItem::Type_Podcast:
            return true;
        #endif
        default:
            return false;
        }
    #ifdef ENABLE_UBUNTU
    case Cantata::Role_Image:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            return static_cast<MusicLibraryItemAlbum *>(item)->cover();
        case MusicLibraryItem::Type_Artist:
            return static_cast<MusicLibraryItemArtist *>(item)->cover();
        default:
            return QString();
        }
    #endif
    case Cantata::Role_CoverSong: {
        QVariant v;
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            v.setValue<Song>(static_cast<MusicLibraryItemAlbum *>(item)->coverSong());
            break;
        case MusicLibraryItem::Type_Artist:
            v.setValue<Song>(static_cast<MusicLibraryItemArtist *>(item)->coverSong());
            break;
        #ifdef ENABLE_ONLINE_SERVICES
        case MusicLibraryItem::Type_Podcast:
            v.setValue<Song>(static_cast<MusicLibraryItemPodcast *>(item)->coverSong());
            break;
        #endif
        default:
            break;
        }
        return v;
    }
    case Cantata::Role_TitleText:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return i18nc("Album by Artist", "%1 by %2", item->data(), item->parentItem()->data());
        }
        return item->data();
    default:
        break;
    }
    return ActionModel::data(index, role);
}
