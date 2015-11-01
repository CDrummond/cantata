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
#include "support/localize.h"
#include "support/utils.h"
#include "widgets/icons.h"
#include <QFile>
#include <QTimer>
#include <QStringRef>
#include <QDateTime>
#include <QDir>
#include <QMimeData>
#include <QStringList>
#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

MusicLibraryModel::MusicLibraryModel(QObject *parent)
    : MusicModel(parent)
    , rootItem(new MusicLibraryItemRoot)
{
    rootItem->setModel(this);
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
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

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (Qt::CheckStateRole==role) {
        return static_cast<MusicLibraryItem *>(index.internalPointer())->checkState();
    }

    return MusicModel::data(index, role);
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
            foreach (MusicLibraryItem *album, artistItem->childItems()) {
                if (check!=album->checkState()) {
                    MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
                    QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
                    album->setCheckState(check);
                    foreach (MusicLibraryItem *song, albumItem->childItems()) {
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
            foreach (MusicLibraryItem *song, albumItem->childItems()) {
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

    foreach (MusicLibraryItem *child, parentItem->childItems()) {
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
//    foreach (MusicLibraryItem *artist, rootItem->childItems()) {
//        MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(artist);
//        artistItem->setCheckState(Qt::Checked);

//        foreach (MusicLibraryItem *album, artistItem->childItems()) {
//            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
//            albumItem->setCheckState(Qt::Checked);
//            foreach (MusicLibraryItem *song, albumItem->childItems()) {
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
