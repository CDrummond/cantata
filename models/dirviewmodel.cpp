/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QCommonStyle>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <QMimeData>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "dirviewmodel.h"
#include "dirviewitem.h"
#include "playqueuemodel.h"
#include "itemview.h"

dirViewModel::dirViewModel(QObject *parent)
    : QAbstractItemModel(parent),
      rootItem(new DirViewItemRoot)
{
}

dirViewModel::~dirViewModel()
{
    delete rootItem;
}

QModelIndex dirViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const DirViewItem * parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());

    DirViewItem * const childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex dirViewModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    const DirViewItem * const childItem = static_cast<DirViewItem *>(index.internalPointer());
    DirViewItem * const parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant dirViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int dirViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    const DirViewItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());

    return parentItem->childCount();
}

int dirViewModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<DirViewItem *>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

QVariant dirViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole: {
        QCommonStyle style;
        if (item->type() == DirViewItem::Type_Dir) {
            return QIcon::fromTheme("inode-directory");
        } else if (item->type() == DirViewItem::Type_File) {
            return QIcon::fromTheme("audio-x-generic");
        }
        break;
    }
    case Qt::DisplayRole:
        return item->data(index.column());
    case Qt::ToolTipRole:
        return 0==item->childCount()
            ? item->data(index.column())
            :
                #ifdef ENABLE_KDE_SUPPORT
                i18np("%1\n1 Entry", "%1\n%2 Entries", item->data(index.column()).toString(), item->childCount());
                #else
                (item->childCount()>1
                    ? tr("%1\n%2 Entries").arg(item->data(index.column()).toString()).arg(item->childCount())
                    : tr("%1\n1 Entry").arg(item->data(index.column()).toString()));
                #endif
    case ItemView::Role_SubText:
        switch (item->type()) {
        case DirViewItem::Type_Dir:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Entry", "%1 Entries", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Entry") : tr("%1 Entries").arg(item->childCount());
            #endif
            break;
        case DirViewItem::Type_File:
        default: return QVariant();
        }
    default:
        break;
    }
    return QVariant();
}

void dirViewModel::clear()
{
    updateDirView(new DirViewItemRoot());
}

void dirViewModel::updateDirView(DirViewItemRoot *newroot)
{
    const DirViewItemRoot *oldRoot = rootItem;

    beginResetModel();
    rootItem = newroot;
    delete oldRoot;
    endResetModel();
}

Qt::ItemFlags dirViewModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsDropEnabled;
}

QMimeData *dirViewModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList filenames;

    foreach(QModelIndex index, indexes) {
        DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());
        recurseDirItems(*item, filenames);
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    return mimeData;
}

void dirViewModel::recurseDirItems(DirViewItem &parent, QStringList &filenames) const
{
    if (parent.childCount() == 0) {
        if (!filenames.contains(parent.fileName())) {
            filenames << parent.fileName();
        }
    } else {
        for (int i = 0; i < parent.childCount(); i++) {
            recurseDirItems(*parent.child(i), filenames);
        }
    }
}
