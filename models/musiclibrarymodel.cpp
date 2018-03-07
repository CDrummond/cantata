/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "roles.h"
#include "support/utils.h"
#include "widgets/icons.h"
#include "gui/settings.h"
#include <QFile>
#include <QTimer>
#include <QStringRef>
#include <QDateTime>
#include <QDir>
#include <QMimeData>
#include <QStringList>

MusicLibraryModel::MusicLibraryModel(QObject *parent)
    : ActionModel(parent)
    , rootItem(new MusicLibraryItemRoot)
{
    rootItem->setModel(this);
}

MusicLibraryModel::~MusicLibraryModel()
{
    delete rootItem;
}

QModelIndex MusicLibraryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const MusicLibraryItem * parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());
    }

    MusicLibraryItem * const childItem = parentItem->childItem(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

QModelIndex MusicLibraryModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    const MusicLibraryItem * const childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem * const parentItem = childItem->parentItem();

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int MusicLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const MusicLibraryItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

const MusicLibraryItemRoot * MusicLibraryModel::root(const MusicLibraryItem *item) const
{
    if (MusicLibraryItem::Type_Root==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item);
    }

    if (MusicLibraryItem::Type_Artist==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item->parentItem());
    }

    if (MusicLibraryItem::Type_Album==item->itemType()) {
        return static_cast<const MusicLibraryItemRoot *>(item->parentItem()->parentItem());
    }

    if (MusicLibraryItem::Type_Song==item->itemType()) {
        return root(item->parentItem());
    }

    return nullptr;
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

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (Qt::CheckStateRole==role) {
        return static_cast<MusicLibraryItem *>(index.internalPointer())->checkState();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Root:
            return static_cast<MusicLibraryItemRoot *>(item)->icon();
        case MusicLibraryItem::Type_Artist:
            return Icons::self()->artistIcon;
        case MusicLibraryItem::Type_Album:
            return Icons::self()->albumIcon(32, true);
        case MusicLibraryItem::Type_Song:
            return Song::Playlist==static_cast<MusicLibraryItemSong *>(item)->song().type ? Icons::self()->playlistListIcon : Icons::self()->audioListIcon;
        default:
            return QVariant();
        }
    case Cantata::Role_BriefMainText:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return item->data();
        }
    case Cantata::Role_MainText:
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (Song::Playlist==song->song().type) {
                return song->song().isCueFile() ? tr("Cue Sheet") : tr("Playlist");
            }
            if (MusicLibraryItem::Type_Root==song->parentItem()->itemType()) {
                return song->song().trackAndTitleStr();
            }
            return song->song().trackAndTitleStr();
        }
        return item->displayData();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        if (MusicLibraryItem::Type_Song==item->itemType()) {
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
                return tr("%n Track(s)", "", item->childCount());
            }
            return tr("%n Artist(s)", "", item->childCount());
        }
        case MusicLibraryItem::Type_Artist:
            return tr("%n Album(s)", "", item->childCount());
        case MusicLibraryItem::Type_Song:
            return Utils::formatTime(static_cast<MusicLibraryItemSong *>(item)->time(), true);
        case MusicLibraryItem::Type_Album:
            return tr("%n Tracks (%1)", "", static_cast<MusicLibraryItemAlbum *>(item)->trackCount())
                   .arg(Utils::formatTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
        default: return QVariant();
        }
    case Cantata::Role_ListImage:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            return true;
        default:
            return false;
        }
    case Cantata::Role_CoverSong: {
        QVariant v;
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Album:
            v.setValue<Song>(static_cast<MusicLibraryItemAlbum *>(item)->coverSong());
            break;
        case MusicLibraryItem::Type_Artist:
            v.setValue<Song>(static_cast<MusicLibraryItemArtist *>(item)->coverSong());
            break;
        default:
            break;
        }
        return v;
    }
    case Cantata::Role_TitleText:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            return tr("%1 by %2", "Album by Artist").arg(item->data()).arg(item->parentItem()->data());
        }
        return item->data();
    default:
        break;
    }
    return ActionModel::data(index, role);
}

bool MusicLibraryModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (Qt::CheckStateRole==role) {
        if (!idx.isValid()) {
            return false;
        }

        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(idx.internalPointer());
        Qt::CheckState check=value.toBool() ? Qt::Checked : Qt::Unchecked;

        if (item->checkState()==check) {
            return false;
        }

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(item);
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            item->setCheckState(check);
            for (MusicLibraryItem *album: artistItem->childItems()) {
                if (check!=album->checkState()) {
                    MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
                    QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
                    album->setCheckState(check);
                    for (MusicLibraryItem *song: albumItem->childItems()) {
                        song->setCheckState(check);
                    }
                    emit dataChanged(index(0, 0, albumIndex), index(0, albumItem->childCount(), albumIndex));
                }
                emit dataChanged(index(0, 0, artistIndex), index(0, artistItem->childCount(), artistIndex));
            }
            emit dataChanged(idx, idx);
            break;
        }
        case MusicLibraryItem::Type_Album: {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(item->parentItem());
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item);
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            item->setCheckState(check);
            for (MusicLibraryItem *song: albumItem->childItems()) {
                song->setCheckState(check);
            }
            setParentState(artistIndex);
            emit dataChanged(idx, idx);
            break;
        }
        case MusicLibraryItem::Type_Song: {
            item->setCheckState(check);
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item->parentItem());
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(albumItem->parentItem());
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
            setParentState(albumIndex);
            setParentState(artistIndex);
            emit dataChanged(idx, idx);
            break;
        }
        case MusicLibraryItem::Type_Root:
            return false;
        }

        return true;
    }
    return ActionModel::setData(idx, value, role);
}

void MusicLibraryModel::setParentState(const QModelIndex &parent)
{
    MusicLibraryItemContainer *parentItem=static_cast<MusicLibraryItemContainer *>(parent.internalPointer());
    Qt::CheckState parentCheck=parentItem->checkState();
    bool haveCheckedChildren=false;
    bool haveUncheckedChildren=false;
    bool stop=false;

    for (MusicLibraryItem *child: parentItem->childItems()) {
        switch (child->checkState()) {
        case Qt::PartiallyChecked:
            parentCheck=Qt::PartiallyChecked;
            stop=true;
            break;
        case Qt::Unchecked:
            haveUncheckedChildren=true;
            parentCheck=haveCheckedChildren ? Qt::PartiallyChecked : Qt::Unchecked;
            stop=haveCheckedChildren && haveUncheckedChildren;
            break;
        case Qt::Checked:
            haveCheckedChildren=true;
            parentCheck=haveUncheckedChildren ? Qt::PartiallyChecked : Qt::Checked;
            stop=haveCheckedChildren && haveUncheckedChildren;
            break;
        }
        if (stop) {
            break;
        }
    }

    if (parentItem->checkState()!=parentCheck) {
        parentItem->setCheckState(parentCheck);
        emit dataChanged(parent, parent);
    }
}

void MusicLibraryModel::clear()
{
    beginResetModel();
    rootItem->clear();
    endResetModel();
}

void MusicLibraryModel::setSongs(const QSet<Song> &songs)
{
    rootItem->update(songs);
//    for (MusicLibraryItem *artist: rootItem->childItems()) {
//        MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(artist);
//        artistItem->setCheckState(Qt::Checked);

//        for (MusicLibraryItem *album: artistItem->childItems()) {
//            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
//            albumItem->setCheckState(Qt::Checked);
//            for (MusicLibraryItem *song: albumItem->childItems()) {
//                song->setCheckState(Qt::Checked);
//            }
//        }
//    }
}

Qt::ItemFlags MusicLibraryModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    }
    return Qt::ItemIsDropEnabled;
}

#include "moc_musiclibrarymodel.cpp"
