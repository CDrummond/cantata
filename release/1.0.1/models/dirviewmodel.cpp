/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <QMimeData>
#include "localize.h"
#include "dirviewmodel.h"
#include "dirviewitem.h"
#include "playqueuemodel.h"
#include "itemview.h"
#include "settings.h"
#include "mpdconnection.h"
#include "icon.h"

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(DirViewModel, instance)
#endif

DirViewModel * DirViewModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static DirViewModel *instance=0;
    if(!instance) {
        instance=new DirViewModel;
    }
    return instance;
    #endif
}

DirViewModel::DirViewModel(QObject *parent)
    : ActionModel(parent)
    , rootItem(new DirViewItemRoot)
    , enabled(false)
{
}

DirViewModel::~DirViewModel()
{
    delete rootItem;
}

void DirViewModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), this, SLOT(updateDirView(DirViewItemRoot *)));
    } else {
        disconnect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), this, SLOT(updateDirView(DirViewItemRoot *)));
    }
}

QModelIndex DirViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const DirViewItem * parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());
    }

    DirViewItem * const childItem = parentItem->child(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

QModelIndex DirViewModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    const DirViewItem * const childItem = static_cast<DirViewItem *>(index.internalPointer());
    DirViewItem * const parentItem = childItem->parent();

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant DirViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int DirViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const DirViewItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<DirViewItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int DirViewModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant DirViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    DirViewItem *item = static_cast<DirViewItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole: {
        if (item->type() == DirViewItem::Type_Dir) {
            return Icon("inode-directory");
        } else if (item->type() == DirViewItem::Type_File) {
            return Icon("audio-x-generic");
        }
        break;
    }
    case Qt::DisplayRole:
        return item->data();
    case Qt::ToolTipRole:
        return 0==item->childCount()
            ? item->data()
            : item->data()+"\n"+
                #ifdef ENABLE_KDE_SUPPORT
                i18np("1 Entry", "%1 Entries", item->childCount());
                #else
                QTP_ENTRIES_STR(item->childCount());
                #endif
    case ItemView::Role_SubText:
        switch (item->type()) {
        case DirViewItem::Type_Dir:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Entry", "%1 Entries", item->childCount());
            #else
            return QTP_ENTRIES_STR(item->childCount());
            #endif
            break;
        case DirViewItem::Type_File:
            if (item->data().endsWith(".asx", Qt::CaseInsensitive) || item->data().endsWith(".m3u", Qt::CaseInsensitive) ||
                       item->data().endsWith(".pls", Qt::CaseInsensitive) || item->data().endsWith(".xspf", Qt::CaseInsensitive) ) {
                return i18n("Playlist");
            } else if (item->data().endsWith(".cue", Qt::CaseInsensitive)) {
                return i18n("Cue Sheet");
            } else {
                return i18n("Audio File");
            }
        default:
            return QVariant();
        }
    default:
        return ActionModel::data(index, role);
    }
    return QVariant();
}

void DirViewModel::clear()
{
    updateDirView(new DirViewItemRoot());
}

void DirViewModel::updateDirView(DirViewItemRoot *newroot)
{
    bool incremental=enabled && rootItem->childCount() && newroot->childCount();

    if (incremental) {
        QSet<QString> currentFiles=rootItem->allFiles();
        QSet<QString> updateFiles=newroot->allFiles();
        QSet<QString> removed=currentFiles-updateFiles;
        QSet<QString> added=updateFiles-currentFiles;

        foreach (const QString &s, added) {
            addFileToList(s);
        }
        foreach (const QString &s, removed) {
            removeFileFromList(s);
        }
    } else {
        const DirViewItemRoot *oldRoot = rootItem;

        beginResetModel();
        rootItem = newroot;
        delete oldRoot;
        endResetModel();
    }
}

void DirViewModel::addFileToList(const QString &file)
{
    if (!enabled) {
        return;
    }
    addFileToList(file.split('/'), QModelIndex(), rootItem);
}

void DirViewModel::removeFileFromList(const QString &file)
{
    if (!enabled) {
        return;
    }
    removeFileFromList(file.split('/'), QModelIndex(), rootItem);
}

void DirViewModel::addFileToList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir)
{
    if (0==parts.count()) {
        return;
    }

    QString p=parts[0];

    DirViewItem *child=dir->child(p);
    if (child) {
        if (DirViewItem::Type_Dir==child->type()) {
            addFileToList(parts.mid(1), index(dir->indexOf(child), 0, parent), static_cast<DirViewItemDir *>(child));
        }
    } else {
        beginInsertRows(parent, dir->childCount(), dir->childCount());
        dir->insertFile(parts);
        endInsertRows();
    }
}

void DirViewModel::removeFileFromList(const QStringList &parts, const QModelIndex &parent, DirViewItemDir *dir)
{
    if (0==parts.count()) {
        return;
    }

    QString p=parts[0];

    DirViewItem *child=dir->child(p);
    if (child) {
        if (DirViewItem::Type_Dir==child->type()) {
            removeFileFromList(parts.mid(1), index(dir->indexOf(child), 0, parent), static_cast<DirViewItemDir *>(child));
        } else if (DirViewItem::Type_File==child->type()) {
            int index=dir->indexOf(child);
            beginRemoveRows(parent, index, index);
            dir->remove(child);
            endRemoveRows();
        }
    }

    if (0==dir->childCount() && dir->parent()) {
        DirViewItemDir *pd=static_cast<DirViewItemDir *>(dir->parent());
        int index=pd->indexOf(dir);
        beginRemoveRows(parent.parent(), index, index);
        pd->remove(dir);
        delete dir;
        endRemoveRows();
    }
}

Qt::ItemFlags DirViewModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsDropEnabled;
    }
}

QStringList DirViewModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QStringList fnames;

    foreach(QModelIndex index, indexes) {
        getFiles(static_cast<DirViewItem *>(index.internalPointer()), fnames, allowPlaylists);
    }
    return fnames;
}

QMimeData *DirViewModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << MPDConnection::self()->getDetails().dir+f;
        }
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}

static inline void addFile(DirViewItem *item, QStringList &insertInto, QStringList &checkAgainst, bool allowPlaylists)
{
    QString path=item->fullName();
    if ((allowPlaylists || !MPDConnection::isPlaylist(path)) && !checkAgainst.contains(path)) {
        insertInto << path;
    }
}

void DirViewModel::getFiles(DirViewItem *item, QStringList &filenames, bool allowPlaylists) const
{
    if (!item) {
        return;
    }

    switch (item->type()) {
        case DirViewItem::Type_File:
            addFile(item, filenames, filenames, allowPlaylists);
        break;
        case DirViewItem::Type_Dir: {
            QStringList dirFiles;
            for (int c=0; c<item->childCount(); c++) {
                DirViewItem *child=item->child(c);
                if (DirViewItem::Type_Dir==child->type()) {
                    getFiles(child, filenames, allowPlaylists);
                } else if (DirViewItem::Type_File==child->type()) {
                    addFile(child, dirFiles, filenames, allowPlaylists);
                }
            }

            qSort(dirFiles);
            filenames+=dirFiles;
        }
        default:
        break;
    }
}
