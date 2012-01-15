/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QModelIndex>
#include <QtGui/QMenu>
#include "playlistsmodel.h"
#include "itemview.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(PlaylistsModel, instance)
#endif
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdconnection.h"
#include "playqueuemodel.h"

PlaylistsModel * PlaylistsModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static PlaylistsModel *instance=0;;
    if(!instance) {
        instance=new PlaylistsModel;
    }
    return instance;
    #endif
}

PlaylistsModel::PlaylistsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , itemMenu(0)
{
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(const QList<Playlist> &)), this, SLOT(setPlaylists(const QList<Playlist> &)));
    connect(MPDConnection::self(), SIGNAL(playlistInfoRetrieved(const QString &, const QList<Song> &)), this, SLOT(playlistInfoRetrieved(const QString &, const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(removedFromPlaylist(const QString &, const QList<int> &)), this, SLOT(removedFromPlaylist(const QString &, const QList<int> &)));
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(const QString &)), MPDConnection::self(), SLOT(playlistInfo(const QString &)));
    connect(this, SIGNAL(addToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(this, SIGNAL(moveInPlaylist(const QString &, int, int)), MPDConnection::self(), SLOT(moveInPlaylist(const QString &, int, int)));
    connect(MPDConnection::self(), SIGNAL(movedInPlaylist(const QString &, int, int)), this, SLOT(movedInPlaylist(const QString &, int, int)));
    updateItemMenu();
}

PlaylistsModel::~PlaylistsModel()
{
    itemMenu->deleteLater();
    itemMenu=0;
}

QVariant PlaylistsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int PlaylistsModel::rowCount(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return items.size();
    }

    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isPlaylist()) {
        PlaylistItem *pl=static_cast<PlaylistItem *>(index.internalPointer());
        if (!pl->loaded) {
            pl->loaded=true;
            emit playlistInfo(pl->name);
        }
        return pl->songs.count();
    }
    return 0;
    //return item->isPlaylist() ? static_cast<PlaylistItem *>(item)->songs.count() : 0;
}

bool PlaylistsModel::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid() || static_cast<Item *>(parent.internalPointer())->isPlaylist();
}

QModelIndex PlaylistsModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if(item->isPlaylist()) {
        return QModelIndex();
    } else {
        SongItem *song=static_cast<SongItem *>(item);

        if (song->parent) {
            return createIndex(items.indexOf(song->parent), 0, song->parent);
        }
    }

    return QModelIndex();
}

QModelIndex PlaylistsModel::index(int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent)) {
        return QModelIndex();
    }

    if (parent.isValid()) {
        Item *p=static_cast<Item *>(parent.internalPointer());

        if (p->isPlaylist()) {
            PlaylistItem *pl=static_cast<PlaylistItem *>(p);
            return row<pl->songs.count() ? createIndex(row, col, pl->songs.at(row)) : QModelIndex();
        }
    }

    return row<items.count() ? createIndex(row, col, items.at(row)) : QModelIndex();
}

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isPlaylist()) {
        PlaylistItem *pl=static_cast<PlaylistItem *>(item);

        switch(role) {
        case Qt::DisplayRole:
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            return pl->name;
        case Qt::ToolTipRole:
            return 0==pl->songs.count()
                ? pl->name
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Track", "%1\n%2 Tracks", pl->name, pl->songs.count());
                    #else
                    (pl->songs.count()>1
                        ? tr("%1\n%2 Tracks").arg(pl->name).arg(pl->songs.count())
                        : tr("%1\n1 Track").arg(pl->name));
                    #endif
        case Qt::DecorationRole:
            return QIcon::fromTheme("view-media-playlist");
        case ItemView::Role_SubText:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track", "%1 Tracks", pl->songs.count());
            #else
            return (pl->songs.count()>1
                ? tr("%1 Tracks").arg(pl->songs.count())
                : tr("1 Track"));
            #endif
        default: break;
        }
    } else {
        SongItem *s=static_cast<SongItem *>(item);

        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole: {
            QString text=s->entryName();

            if (Qt::ToolTipRole==role && !s->title.isEmpty()) {
                QString duration=MPDParseUtils::formatDuration(s->time);
                if (duration.startsWith(QLatin1String("00:"))) {
                    duration=duration.mid(3);
                }
                if (duration.startsWith(QLatin1String("00:"))) {
                    duration=duration.mid(1);
                }
                text=text+QChar('\n')+duration;
            }
            return text;
        }
        case Qt::DecorationRole:
            return QIcon::fromTheme(s->title.isEmpty() ? "applications-internet" : "audio-x-generic");
        case ItemView::Role_SubText:
            if (!s->title.isEmpty()) {
                QString duration=MPDParseUtils::formatDuration(s->time);
                if (duration.startsWith(QLatin1String("00:"))) {
                    duration=duration.mid(3);
                }
                if (duration.startsWith(QLatin1String("00:"))) {
                    duration=duration.mid(1);
                }
                return duration;
            }
            return QString();
        }
    }

    return QVariant();
}

Qt::ItemFlags PlaylistsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
//         if (static_cast<Item *>(index.internalPointer())->isPlaylist()) {
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
//         } else {
//             return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
//         }
    } else {
        return 0;
    }
}

Qt::DropActions PlaylistsModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList PlaylistsModel::filenames(const QModelIndexList &indexes) const
{
    QStringList fnames;
    QSet<Item *> selectedPlaylists;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            selectedPlaylists.insert(item);
            foreach (const SongItem *s, static_cast<PlaylistItem*>(item)->songs) {
                fnames << s->file;
            }
        } else if (!selectedPlaylists.contains(static_cast<SongItem*>(item)->parent)) {
            fnames << static_cast<SongItem*>(item)->file;
        }
    }

    return fnames;
}

static const QLatin1String constPlaylistNameMimeType("cantata/playlistnames");
static const QLatin1String constPositionsMimeType("cantata/positions");

QMimeData * PlaylistsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList filenames;
    QStringList playlists;
    QStringList positions;
    QSet<Item *> selectedPlaylists;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            int pos=0;
            selectedPlaylists.insert(item);
            foreach (const SongItem *s, static_cast<PlaylistItem*>(item)->songs) {
                filenames << s->file;
                playlists << static_cast<PlaylistItem*>(item)->name;
                positions << QString::number(pos++);
            }
        } else if (!selectedPlaylists.contains(static_cast<SongItem*>(item)->parent)) {
            filenames << static_cast<SongItem*>(item)->file;
            playlists << static_cast<SongItem*>(item)->parent->name;
            positions << QString::number(static_cast<SongItem*>(item)->parent->songs.indexOf(static_cast<SongItem*>(item)));
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    PlayQueueModel::encode(*mimeData, constPlaylistNameMimeType, playlists);
    PlayQueueModel::encode(*mimeData, constPositionsMimeType, positions);
    return mimeData;
}

bool PlaylistsModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int col, const QModelIndex &parent)
{
    Q_UNUSED(row)
    Q_UNUSED(col)

    if (Qt::IgnoreAction==action) {
        return true;
    }

    if (!parent.isValid()) {
        return false;
    }

    if (data->hasFormat(PlayQueueModel::constFileNameMimeType)) {
        Item *item=static_cast<Item *>(parent.internalPointer());
        PlaylistItem *pl=item->isPlaylist()
                            ? static_cast<PlaylistItem *>(item)
                            : static_cast<SongItem *>(item)->parent;

        if (!pl) {
            return false;
        }

        QStringList filenames=PlayQueueModel::decode(*data, PlayQueueModel::constFileNameMimeType);

        if (data->hasFormat(PlayQueueModel::constMoveMimeType)) {
            emit addToPlaylist(pl->name, filenames);
            if (!item->isPlaylist()) {
                for (int i=0; i<filenames.count(); ++i) {
                    emit moveInPlaylist(pl->name, pl->songs.count(), parent.row());
                }
            }
            return true;
        } else if (data->hasFormat(constPlaylistNameMimeType)) {
            QStringList playlists=PlayQueueModel::decode(*data, constPlaylistNameMimeType);

            bool fromThis=false;
            bool fromOthers=false;
            foreach (const QString &p, playlists) {
                if (p==pl->name) {
                    fromThis=true;
                } else {
                    fromOthers=true;
                }

                if (fromThis && fromOthers) {
                    return false; // Weird mix...
                }
            }
            if (fromThis) {
                if (!item->isPlaylist()) {
                    QStringList list=PlayQueueModel::decode(*data, constPositionsMimeType);
                    QList<int> pos;

                    foreach (const QString &s, list) {
                        pos.append(s.toUInt());
                    }
                    qSort(pos);
                    int offset=0;
                    foreach (int p, pos) {
                        int dest=parent.row();
                        int source=p;

                        if (dest>source) {
                            source-=offset++;
                        }
                        emit moveInPlaylist(pl->name, source, dest);
                    }
                    return true;
                }
            } else {
                emit addToPlaylist(pl->name, filenames);
                if (!item->isPlaylist()) {
                    for (int i=0; i<filenames.count(); ++i) {
                        emit moveInPlaylist(pl->name, pl->songs.count(), parent.row());
                    }
                }
                return true;
            }
        }
    }

    return false;
}

QStringList PlaylistsModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType;
    return types;
}

void PlaylistsModel::getPlaylists()
{
    emit listPlaylists();
}

void PlaylistsModel::clear()
{
    beginResetModel();
    clearPlaylists();
    updateItemMenu();
    endResetModel();
}

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    if (items.isEmpty()) {
        if (playlists.isEmpty()) {
            return;
        }
        beginResetModel();
        foreach (const Playlist &p, playlists) {
            items.append(new PlaylistItem(p));
        }
        endResetModel();
        updateItemMenu();
    } else if (playlists.isEmpty()) {
        clear();
    } else {
        QModelIndex parent=QModelIndex();
        QSet<QString> existing;
        QSet<QString> retreived;
        QSet<QString> removed;
        QSet<QString> added;

        foreach (PlaylistItem *p, items) {
            existing.insert(p->name);
        }

        foreach (const Playlist &p, playlists) {
            retreived.insert(p.name);
        }

        removed=existing-retreived;
        added=retreived-existing;

        if (removed.count()) {
            foreach (const QString &p, removed) {
                PlaylistItem *pl=getPlaylist(p);
                if (pl) {
                    int index=items.indexOf(pl);
                    beginRemoveRows(parent, index, index);
                    delete items.takeAt(index);
                    endRemoveRows();
                }
            }
        }
        if (added.count()) {
            beginInsertRows(parent, items.count(), items.count()+added.count()-1);
            foreach (const QString &p, added) {
                items.append(new PlaylistItem(p));
            }
            endInsertRows();
        }
    }
}

void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    PlaylistItem *pl=getPlaylist(name);

    if (pl) {
        if (pl->songs.isEmpty()) {
            if (songs.isEmpty()) {
                return;
            }
            beginInsertRows(createIndex(items.indexOf(pl), 0, pl), 0, songs.count()-1);
            foreach (const Song &s, songs) {
                pl->songs.append(new SongItem(s, pl));
            }
            endInsertRows();
            updateItemMenu();
        } else if (songs.isEmpty()) {
            beginRemoveRows(createIndex(items.indexOf(pl), 0, pl), 0, pl->songs.count()-1);
            pl->clearSongs();
            endRemoveRows();
        } else {
            QModelIndex parent=createIndex(items.indexOf(pl), 0, pl);
            int count=pl->songs.count();
            int addFrom=songs.count()>count ? count : -1;
            for (int i=0; i<count; ++i) {
                if (i>=songs.count()) {
                    // Remove all remaining...
                    beginRemoveRows(parent, i, count);
                    for (int j=i; j<count; ++j) {
                        delete pl->songs.takeAt(i);
                    }
                    endRemoveRows();
                    break;
                } else {
                    Song n=songs.at(i);
                    const SongItem *o=pl->songs.at(i);

                    if (o->file!=n.file) {
                        addFrom=i;
                        beginRemoveRows(parent, i, count);
                        for (int j=i; j<count; ++j) {
                            delete pl->songs.takeAt(i);
                        }
                        endRemoveRows();
                        break;
                    }
                }
            }

            if (addFrom>=0) {
                beginInsertRows(parent, pl->songs.count(), pl->songs.count()+(songs.count()-(1+addFrom)));
                for (int i=addFrom; i<songs.count(); ++i) {
                    pl->songs.append(new SongItem(songs.at(i), pl));
                }
                endInsertRows();
            }
        }
        QModelIndex idx=index(items.indexOf(pl), 0, QModelIndex());
        emit dataChanged(idx, idx);
    } else {
        emit listPlaylists();
    }
}

void PlaylistsModel::removedFromPlaylist(const QString &name, const QList<int> &positions)
{
    PlaylistItem *pl=0;
    if (0==positions.count() || !(pl=getPlaylist(name))) {
        emit listPlaylists();
        return;
    }

    int adjust=0;
    QModelIndex parent=createIndex(items.indexOf(pl), 0, pl);
    QList<int>::ConstIterator it=positions.constBegin();
    QList<int>::ConstIterator end=positions.constEnd();
    while(it!=end) {
        int rowBegin=*it;
        int rowEnd=*it;
        QList<int>::ConstIterator next=it+1;
        while(next!=end) {
            if (*next!=(rowEnd+1)) {
                break;
            } else {
                it=next;
                rowEnd=*next;
                next++;
            }
        }
        beginRemoveRows(parent, rowBegin-adjust, rowEnd-adjust);
        for (int i=rowBegin; i<=rowEnd; ++i) {
            delete pl->songs.takeAt(rowBegin-adjust);
        }
        adjust+=(rowEnd-rowBegin)+1;
        endRemoveRows();
        it++;
    }
}

void PlaylistsModel::movedInPlaylist(const QString &name, int from, int to)
{
    PlaylistItem *pl=0;
    if (!(pl=getPlaylist(name)) || from>pl->songs.count()) {
        emit listPlaylists();
        return;
    }
    QModelIndex parent=createIndex(items.indexOf(pl), 0, pl);
    beginRemoveRows(parent, from, from);
    SongItem *si=pl->songs.takeAt(from);
    endRemoveRows();
    beginInsertRows(parent, to<from ? to : to-1, to<from ? to : to-1);
    pl->songs.insert(to, si);
    endInsertRows();
}

static QString qt_strippedText(QString s)
{
    s.remove(QString::fromLatin1("..."));
    int i = 0;
    while (i < s.size()) {
        ++i;
        if (s.at(i - 1) != QLatin1Char('&')) {
            continue;
        }

        if (i < s.size() && s.at(i) == QLatin1Char('&')) {
            ++i;
        }
        s.remove(i - 1, 1);
    }
    return s.trimmed();
}

void PlaylistsModel::emitAddToExisting()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToExisting(qt_strippedText(act->text()));
    }
}

void PlaylistsModel::updateItemMenu()
{
    if (!itemMenu) {
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();
    #ifdef ENABLE_KDE_SUPPORT
    itemMenu->addAction(QIcon::fromTheme("document-new"), i18n("New Playlist..."), this, SIGNAL(addToNew()));
    #else
    itemMenu->addAction(QIcon::fromTheme("document-new"), tr("New Playlist..."), this, SIGNAL(addToNew()));
    #endif

    QStringList names;
    foreach (const PlaylistItem *p, items) {
        names << p->name;
    }
    qSort(names);
    foreach (const QString &n, names) {
        itemMenu->addAction(n, this, SLOT(emitAddToExisting()));
    }
}

PlaylistsModel::PlaylistItem * PlaylistsModel::getPlaylist(const QString &name)
{
    foreach (PlaylistItem *p, items) {
        if (p->name==name) {
            return p;
        }
    }

    return 0;
}

void PlaylistsModel::clearPlaylists()
{
    qDeleteAll(items);
    items.clear();
}

PlaylistsModel::PlaylistItem::~PlaylistItem()
{
    clearSongs();
}

void PlaylistsModel::PlaylistItem::clearSongs()
{
    qDeleteAll(songs);
    songs.clear();
}

PlaylistsModel::SongItem * PlaylistsModel::PlaylistItem::getSong(const Song &song)
{
    foreach (SongItem *s, songs) {
        if (*s==song) {
            return s;
        }
    }

    return 0;
}
