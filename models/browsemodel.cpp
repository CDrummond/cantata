/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "browsemodel.h"
#include "roles.h"
#include "playqueuemodel.h"
#include "widgets/icons.h"
#include "gui/settings.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdstats.h"
#include "support/monoicon.h"
#include "support/utils.h"
#include <QMimeData>

void BrowseModel::FolderItem::add(Item *i)
{
    i->setRow(children.count());
    children.append(i);
}

QStringList BrowseModel::FolderItem::allEntries(bool allowPlaylists) const
{
    QStringList entries;
    if (children.isEmpty()) {
        entries << MPDConnection::constDirPrefix+path;
    } else {
        for (Item *i: children) {
            if (i->isFolder()) {
                entries+=static_cast<FolderItem *>(i)->allEntries(allowPlaylists);
            } else if (allowPlaylists || Song::Playlist!=static_cast<TrackItem *>(i)->getSong().type) {
                entries+=static_cast<TrackItem *>(i)->getSong().file;
            }
        }
    }
    return entries;
}

BrowseModel::BrowseModel(QObject *p)
    : ActionModel(p)
    , root(new FolderItem("/", nullptr))
    , enabled(false)
    , dbVersion(0)
{
    icn=MonoIcon::icon(FontAwesome::server, Utils::monoIconColor());
    connect(this, SIGNAL(listFolder(QString)), MPDConnection::self(), SLOT(listFolder(QString)));
    folderIndex.insert(root->getPath(), root);
}

QString BrowseModel::name() const
{
    return QLatin1String("mpdbrowse");
}

QString BrowseModel::title() const
{
    return tr("Server Folders");
}

QString BrowseModel::descr() const
{
    return tr("MPD virtual file-system");
}

void BrowseModel::clear()
{
    beginResetModel();
    root->clear();
    folderIndex.clear();
    folderIndex.insert(root->getPath(), root);
    endResetModel();
}

void BrowseModel::load()
{
    if (!enabled || (root && (root->getChildCount() || root->isFetching()))) {
        return;
    }
    root->setState(FolderItem::State_Fetching);
    emit listFolder(root->getPath());
}

void BrowseModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(folderContents(QString,QStringList,QList<Song>)), this, SLOT(folderContents(QString,QStringList,QList<Song>)));
        connect(MPDConnection::self(), SIGNAL(connectionChanged(MPDConnectionDetails)), this, SLOT(connectionChanged()));
        connect(MPDConnection::self(), SIGNAL(statsUpdated(MPDStatsValues)), this, SLOT(statsUpdated(MPDStatsValues)));
    } else {
        disconnect(MPDConnection::self(), SIGNAL(folderContents(QString,QStringList,QList<Song>)), this, SLOT(folderContents(QString,QStringList,QList<Song>)));
        disconnect(MPDConnection::self(), SIGNAL(connectionChanged(MPDConnectionDetails)), this, SLOT(connectionChanged()));
        disconnect(MPDConnection::self(), SIGNAL(statsUpdated(MPDStatsValues)), this, SLOT(statsUpdated(MPDStatsValues)));
        clear();
    }
}

Qt::ItemFlags BrowseModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    }
    return Qt::ItemIsDropEnabled;
}

QModelIndex BrowseModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    const FolderItem * p = parent.isValid() ? static_cast<FolderItem *>(parent.internalPointer()) : root;
    const Item * c = row<p->getChildCount() ? p->getChildren().at(row) : nullptr;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex BrowseModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }

    const Item * const item = static_cast<Item *>(child.internalPointer());
    Item * const parentItem = item->getParent();
    if (parentItem == root || nullptr==parentItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->getRow(), 0, parentItem);
}

int BrowseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const FolderItem *parentItem=parent.isValid() ? static_cast<FolderItem *>(parent.internalPointer()) : root;
    return parentItem ? parentItem->getChildCount() : 0;
}

int BrowseModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

bool BrowseModel::hasChildren(const QModelIndex &index) const
{
    Item *item=toItem(index);
    return item && item->isFolder();
}

bool BrowseModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        Item *item = toItem(index);
        return item && item->isFolder() && static_cast<FolderItem *>(item)->canFetchMore();
    } else {
        return false;
    }
}

void BrowseModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    FolderItem *item = static_cast<FolderItem *>(toItem(index));
    if (!item->isFetching()) {
        item->setState(FolderItem::State_Fetching);
        emit listFolder(item->getPath());
    }
}

QVariant BrowseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return descr();
        case Qt::DecorationRole:
            return icon();
        }
        return QVariant();
    }

    Item *item = static_cast<Item *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        if (item->isFolder()) {
            return Icons::self()->folderListIcon;
        } else {
            TrackItem *track = static_cast<TrackItem *>(item);
            return Song::Playlist==track->getSong().type ? Icons::self()->playlistListIcon : Icons::self()->audioListIcon;
        }
        break;
    case Cantata::Role_BriefMainText:
    case Cantata::Role_MainText:
    case Qt::DisplayRole:
        if (!item->isFolder()) {
            TrackItem *track = static_cast<TrackItem *>(item);
            if (Song::Playlist==track->getSong().type) {
                return Utils::getFile(track->getSong().file);
            }
        }
        return item->getText();
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        if (item->isFolder() || Song::Playlist==static_cast<TrackItem *>(item)->getSong().type) {
            return static_cast<FolderItem *>(item)->getPath();
        }
        return static_cast<TrackItem *>(item)->getSong().toolTip();
    case Cantata::Role_SubText:
        if (!item->isFolder()) {
            TrackItem *track = static_cast<TrackItem *>(item);
            if (Song::Playlist==track->getSong().type) {
                return track->getSong().isCueFile() ? tr("Cue Sheet") : tr("Playlist");
            }
        }
        return item->getSubText();
    case Cantata::Role_TitleText:
        return item->getText();
    case Cantata::Role_TitleActions:
        return item->isFolder();
    default:
        break;
    }
    return ActionModel::data(index, role);
}

QMimeData * BrowseModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files;
    for (const QModelIndex &idx: indexes) {
        Item *item=toItem(idx);
        if (item) {
            if (item->isFolder()) {
                files+=static_cast<FolderItem *>(item)->allEntries(false);
            } else {
                files.append(static_cast<TrackItem *>(item)->getSong().file);
            }
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    return mimeData;
}

QList<Song> BrowseModel::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> songList;
    for (const QModelIndex &idx: indexes) {
        Item *item=toItem(idx);
        if (item && !item->isFolder() && (allowPlaylists || Song::Standard==static_cast<TrackItem *>(item)->getSong().type)) {
            songList.append(static_cast<TrackItem *>(item)->getSong());
        }
    }
    return songList;
}

void BrowseModel::connectionChanged()
{
    clear();
    if (!root->isFetching() && MPDConnection::self()->isConnected()) {
        dbVersion=0;
        root->setState(FolderItem::State_Fetching);
        emit listFolder(root->getPath());
    }
}

void BrowseModel::statsUpdated(const MPDStatsValues &stats)
{
    if (stats.dbUpdate!=dbVersion) {
        if (0!=dbVersion) {
            connectionChanged();
        }
        dbVersion=stats.dbUpdate;
    }
}

void BrowseModel::folderContents(const QString &path, const QStringList &folders, const QList<Song> &songs)
{
    QMap<QString, FolderItem *>::Iterator it=folderIndex.find(path);
    if (it==folderIndex.end() || 0!=it.value()->getChildCount()) {
        return;
    }

	if (folders.count() + songs.count()) {
		QModelIndex idx=it.value()==root ? QModelIndex() : createIndex(it.value()->getRow(), 0, it.value());

		beginInsertRows(idx, 0, folders.count() + songs.count() - 1);
        for (const QString &folder: folders) {
			FolderItem *item = new FolderItem(folder.split("/", QString::SkipEmptyParts).last(), folder, it.value());
			it.value()->add(item);
			folderIndex.insert(folder, item);
		}
        for (const Song &song: songs) {
			it.value()->add(new TrackItem(song, it.value()));
		}
		it.value()->setState(FolderItem::State_Fetched);
		endInsertRows();
	}
}

#include "moc_browsemodel.cpp"
