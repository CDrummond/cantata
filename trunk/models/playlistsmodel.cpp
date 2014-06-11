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

#include <QModelIndex>
#include <QMimeData>
#include <QMenu>
#include <QFont>
#include "config.h"
#include "playlistsmodel.h"
#include "playlistsproxymodel.h"
#include "playqueuemodel.h"
#include "widgets/groupedview.h"
#include "roles.h"
#include "gui/plurals.h"
#include "support/localize.h"
#include "support/utils.h"
#include "support/globalstatic.h"
#include "mpd/mpdparseutils.h"
#include "mpd/mpdstats.h"
#include "mpd/mpdconnection.h"
#include "playqueuemodel.h"
#include "streamsmodel.h"
#include "widgets/icons.h"
#ifdef ENABLE_HTTP_SERVER
#include "http/httpserver.h"
#endif

#if defined ENABLE_MODEL_TEST
#include "modeltest.h"
#endif

#ifndef ENABLE_UBUNTU
QString PlaylistsModel::headerText(int col)
{
    switch (col) {
    case COL_TITLE:     return PlayQueueModel::headerText(PlayQueueModel::COL_TITLE);
    case COL_ARTIST:    return PlayQueueModel::headerText(PlayQueueModel::COL_ARTIST);
    case COL_ALBUM:     return PlayQueueModel::headerText(PlayQueueModel::COL_ALBUM);
    case COL_LENGTH:    return PlayQueueModel::headerText(PlayQueueModel::COL_LENGTH);
    case COL_YEAR:      return PlayQueueModel::headerText(PlayQueueModel::COL_YEAR);
    case COL_GENRE:     return PlayQueueModel::headerText(PlayQueueModel::COL_GENRE);
    case COL_COMPOSER:  return PlayQueueModel::headerText(PlayQueueModel::COL_COMPOSER);
    case COL_PERFORMER: return PlayQueueModel::headerText(PlayQueueModel::COL_PERFORMER);
    default:            return QString();
    }
}
#endif

GLOBAL_STATIC(PlaylistsModel, instance)

PlaylistsModel::PlaylistsModel(QObject *parent)
    : ActionModel(parent)
    , enabled(true)
    , multiCol(false)
    #ifndef ENABLE_UBUNTU
    , itemMenu(0)
    , dropAdjust(0)
    #endif
{
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(const QList<Playlist> &)), this, SLOT(setPlaylists(const QList<Playlist> &)));
    connect(MPDConnection::self(), SIGNAL(playlistInfoRetrieved(const QString &, const QList<Song> &)), this, SLOT(playlistInfoRetrieved(const QString &, const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(removedFromPlaylist(const QString &, const QList<quint32> &)),
            this, SLOT(removedFromPlaylist(const QString &, const QList<quint32> &)));
    connect(MPDConnection::self(), SIGNAL(playlistRenamed(const QString &, const QString &)),
            this, SLOT(playlistRenamed(const QString &, const QString &)));
    connect(MPDConnection::self(), SIGNAL(movedInPlaylist(const QString &, const QList<quint32> &, quint32)),
            this, SLOT(movedInPlaylist(const QString &, const QList<quint32> &, quint32)));
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(const QString &)), MPDConnection::self(), SLOT(playlistInfo(const QString &)));
    connect(this, SIGNAL(addToPlaylist(const QString &, const QStringList, quint32, quint32)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList, quint32, quint32)));
    connect(this, SIGNAL(moveInPlaylist(const QString &, const QList<quint32> &, quint32, quint32)), MPDConnection::self(), SLOT(moveInPlaylist(const QString &, const
    QList<quint32> &, quint32, quint32)));
    #ifndef ENABLE_UBUNTU
    newAction=new QAction(Icon("document-new"), i18n("New Playlist..."), this);
    connect(newAction, SIGNAL(triggered(bool)), this, SIGNAL(addToNew()));
    Action::initIcon(newAction);
    #endif
    #if defined ENABLE_MODEL_TEST
    new ModelTest(this, this);
    #endif
}

PlaylistsModel::~PlaylistsModel()
{
    #ifndef ENABLE_UBUNTU
    if (itemMenu) {
        itemMenu->deleteLater();
        itemMenu=0;
    }
    #endif
}

int PlaylistsModel::rowCount(const QModelIndex &index) const
{
    if (index.column()>0) {
        return 0;
    }

    if (!index.isValid()) {
        return items.size();
    }

    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isPlaylist()) {
        PlaylistItem *pl=static_cast<PlaylistItem *>(index.internalPointer());
        return pl->songs.count();
    }
    return 0;
}

bool PlaylistsModel::canFetchMore(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }
    Item *item=static_cast<Item *>(index.internalPointer());
    return item && item->isPlaylist() && !static_cast<PlaylistItem *>(item)->loaded;
}

void PlaylistsModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    Item *item=static_cast<Item *>(index.internalPointer());
    if (item->isPlaylist() && !static_cast<PlaylistItem *>(item)->loaded) {
        static_cast<PlaylistItem *>(item)->loaded=true;
        emit playlistInfo(static_cast<PlaylistItem *>(item)->name);
    }
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

#ifndef ENABLE_UBUNTU
QVariant PlaylistsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal==orientation) {
        switch (role) {
        case Qt::DisplayRole:
            return headerText(section);
        case Qt::TextAlignmentRole:
            switch (section) {
            case COL_TITLE:
            case COL_ARTIST:
            case COL_ALBUM:
            case COL_GENRE:
            case COL_COMPOSER:
            case COL_PERFORMER:
            default:
                return int(Qt::AlignVCenter|Qt::AlignLeft);
            case COL_LENGTH:
            case COL_YEAR:
                return int(Qt::AlignVCenter|Qt::AlignRight);
            }
        case Cantata::Role_Hideable:
            return COL_YEAR==section || COL_GENRE==section || COL_COMPOSER==section || COL_PERFORMER==section ? true : false;
        case Cantata::Role_Width:
            switch (section) {
            case COL_TITLE:     return 0.4;
            case COL_ARTIST:    return 0.15;
            case COL_ALBUM:     return 0.15;
            case COL_YEAR:      return 0.05;
            case COL_GENRE:     return 0.125;
            case COL_LENGTH:    return 0.125;
            case COL_COMPOSER:  return 0.15;
            case COL_PERFORMER: return 0.15;
            }
        default:
            break;
        }
    }

    return QVariant();
}
#endif

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (Qt::TextAlignmentRole==role) {
        switch (index.column()) {
        case COL_TITLE:
        case COL_ARTIST:
        case COL_ALBUM:
        case COL_GENRE:
        case COL_COMPOSER:
        case COL_PERFORMER:
        default:
            return int(Qt::AlignVCenter|Qt::AlignLeft);
        case COL_LENGTH:
        case COL_YEAR:
            return int(Qt::AlignVCenter|Qt::AlignRight);
        }
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isPlaylist()) {
        PlaylistItem *pl=static_cast<PlaylistItem *>(item);

        switch(role) {
        #ifdef ENABLE_UBUNTU
        case Cantata::Role_Image:
            return QString();
        #endif
        case Cantata::Role_IsCollection:
            return true;
        case Cantata::Role_CollectionId:
            return pl->key;
        case Cantata::Role_Key:
            return 0;
        case Cantata::Role_Song: {
            QVariant var;
            var.setValue<Song>(Song());
            return var;
        }
        case Cantata::Role_AlbumDuration:
            return pl->totalTime();
        case Cantata::Role_SongCount:
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            return pl->songs.count();
        case Cantata::Role_CurrentStatus:
        case Cantata::Role_Status:
            return (int)GroupedView::State_Default;
        case Qt::FontRole:
            if (multiCol) {
                QFont font;
                font.setBold(true);
                return font;
            }
            return QVariant();
        case Cantata::Role_TitleText:
        case Cantata::Role_MainText:
        case Qt::DisplayRole:
            if (multiCol) {
                switch (index.column()) {
                case COL_TITLE:
                    return pl->visibleName();
                case COL_ARTIST:
                case COL_ALBUM:
                    return QVariant();
                case COL_LENGTH:
                    if (!pl->loaded) {
                        pl->loaded=true;
                        emit playlistInfo(pl->name);
                    }
                    return pl->loaded && !pl->isSmartPlaylist ? Utils::formatTime(pl->totalTime()) : QVariant();
                case COL_YEAR:
                case COL_GENRE:
                    return QVariant();
                default:
                    break;
                }
            }
            return pl->visibleName();
        case Qt::ToolTipRole:
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            return 0==pl->songs.count()
                ? pl->visibleName()
                : pl->visibleName()+"\n"+Plurals::tracksWithDuration(pl->songs.count(), Utils::formatTime(pl->totalTime()));
        #ifndef ENABLE_UBUNTU
        case Qt::DecorationRole:
            return multiCol ? QVariant() : (pl->isSmartPlaylist ? Icons::self()->dynamicRuleIcon : Icons::self()->playlistIcon);
        #endif
        case Cantata::Role_SubText:
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            if (pl->isSmartPlaylist) {
                return i18n("Smart Playlist");
            }
            return Plurals::tracksWithDuration(pl->songs.count(), Utils::formatTime(pl->totalTime()));
        default:
            return ActionModel::data(index, role);
        }
    } else {
        SongItem *s=static_cast<SongItem *>(item);

        switch (role) {
        #ifdef ENABLE_UBUNTU
        case Cantata::Role_Image:
            return QString();
        #endif
        case Cantata::Role_IsCollection:
            return false;
        case Cantata::Role_CollectionId:
            return s->parent->key;
        case Cantata::Role_Key:
            return s->key;
        case Cantata::Role_Song: {
            QVariant var;
            var.setValue<Song>(*s);
            return var;
        }
        case Cantata::Role_AlbumDuration: {
            quint32 d=s->time;
            for (int i=index.row()+1; i<s->parent->songs.count(); ++i) {
                const SongItem *song = s->parent->songs.at(i);
                if (song->key!=s->key) {
                    break;
                }
                d+=song->time;
            }
            if (index.row()>1) {
                for (int i=index.row()-1; i<=0; ++i) {
                    const SongItem *song = s->parent->songs.at(i);
                    if (song->key!=s->key) {
                        break;
                    }
                    d+=song->time;
                }
            }
            return d;
        }
        case Cantata::Role_SongCount:{
            quint32 count=1;
            for (int i=index.row()+1; i<s->parent->songs.count(); ++i) {
                const SongItem *song = s->parent->songs.at(i);
                if (song->key!=s->key) {
                    break;
                }
                count++;
            }
            if (index.row()>1) {
                for (int i=index.row()-1; i<=0; ++i) {
                    const SongItem *song = s->parent->songs.at(i);
                    if (song->key!=s->key) {
                        break;
                    }
                    count++;
                }
            }
            return count;
        }
        case Cantata::Role_CurrentStatus:
        case Cantata::Role_Status:
            return (int)GroupedView::State_Default;
        case Qt::DisplayRole:
            if (multiCol) {
                switch (index.column()) {
                case COL_TITLE:
                    return s->trackAndTitleStr(false);
                case COL_ARTIST:
                    return s->artist.isEmpty() ? Song::unknown() : s->artist;
                case COL_ALBUM:
                    return s->album.isEmpty() && !s->name.isEmpty() && s->isStream() ? s->name : s->album;
                case COL_LENGTH:
                    return Utils::formatTime(s->time);
                case COL_YEAR:
                    if (s->year <= 0) {
                        return QVariant();
                    }
                    return s->year;
                case COL_GENRE:
                    return s->genre;
                case COL_COMPOSER:
                    return s->composer();
                case COL_PERFORMER:
                    return s->performer();
                default:
                    break;
                }
            }
        case Qt::ToolTipRole: {
            QString text=s->entryName();

            if (Qt::ToolTipRole==role) {
                text=text.replace("\n", "<br/>");
                if (!s->title.isEmpty()) {
                    text+=QLatin1String("<br/>")+Utils::formatTime(s->time);
                    text+=QLatin1String("<br/><small><i>")+s->file+QLatin1String("</i></small>");
                }
            }
            return text;
        }
        #ifndef ENABLE_UBUNTU
        case Qt::DecorationRole:
            return multiCol ? QVariant() : (s->title.isEmpty() ? Icons::self()->streamIcon : Icons::self()->audioFileIcon);
        #endif
        case Cantata::Role_MainText:
            return s->title.isEmpty() ? s->file : s->title;
        case Cantata::Role_SubText:
            return s->artist+QLatin1String(" - ")+s->displayAlbum();
        default:
            return ActionModel::data(index, role);
        }
    }

    return QVariant();
}

#ifndef ENABLE_UBUNTU
bool PlaylistsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Cantata::Role_DropAdjust==role) {
        dropAdjust=value.toUInt();
        return true;
    } else {
        return QAbstractItemModel::setData(index, value, role);
    }
}
#endif

Qt::ItemFlags PlaylistsModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
    }
    return Qt::NoItemFlags;
}

Qt::DropActions PlaylistsModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList PlaylistsModel::filenames(const QModelIndexList &indexes, bool filesOnly) const
{
    QStringList fnames;
    QSet<Item *> selectedPlaylists;
    foreach(QModelIndex index, indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            selectedPlaylists.insert(item);
            foreach (const SongItem *s, static_cast<PlaylistItem*>(item)->songs) {
                if (!filesOnly || !s->file.contains(':')) {
                    fnames << s->file;
                }
            }
        } else if (!selectedPlaylists.contains(static_cast<SongItem*>(item)->parent)) {
            if (!filesOnly || !static_cast<SongItem*>(item)->file.contains(':')) {
                fnames << static_cast<SongItem*>(item)->file;
            }
        }
    }

    return fnames;
}

QList<Song> PlaylistsModel::songs(const QModelIndexList &indexes) const
{
    QList<Song> songs;
    QSet<Item *> selectedPlaylists;

    foreach (const QModelIndex &idx, indexes) {
        Item *item=static_cast<Item *>(idx.internalPointer());
        if (item->isPlaylist()) {
            PlaylistItem *playlist=static_cast<PlaylistItem *>(item);
            foreach (const SongItem *song, playlist->songs) {
                selectedPlaylists.insert(item);
                songs.append(*song);
            }
        }
    }
    foreach (const QModelIndex &idx, indexes) {
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());
        if (!item->isPlaylist()) {
            SongItem *song=static_cast<SongItem *>(item);
            if (!selectedPlaylists.contains(song->parent)) {
                songs.append(*song);
            }
        }
    }
    return songs;
}

#ifndef ENABLE_UBUNTU
static const QLatin1String constPlaylistNameMimeType("cantata/playlistnames");
static const QLatin1String constPositionsMimeType("cantata/positions");
static const char *constPlaylistProp="hasPlaylist";

QMimeData * PlaylistsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList filenames;
    QStringList playlists;
    QList<quint32> positions;
    QSet<Item *> selectedPlaylists;
    foreach(QModelIndex index, indexes) {
        if (!index.isValid() || 0!=index.column()) {
            continue;
        }

        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            int pos=0;
            selectedPlaylists.insert(item);
            foreach (const SongItem *s, static_cast<PlaylistItem*>(item)->songs) {
                filenames << s->file;
                playlists << static_cast<PlaylistItem*>(item)->name;
                positions << pos++;
            }
            mimeData->setProperty(constPlaylistProp, true);
        } else if (!selectedPlaylists.contains(static_cast<SongItem*>(item)->parent)) {
            filenames << static_cast<SongItem*>(item)->file;
            playlists << static_cast<SongItem*>(item)->parent->name;
            positions << static_cast<SongItem*>(item)->parent->songs.indexOf(static_cast<SongItem*>(item));
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    PlayQueueModel::encode(*mimeData, constPlaylistNameMimeType, playlists);
    PlayQueueModel::encode(*mimeData, constPositionsMimeType, positions);
    return mimeData;
}

bool PlaylistsModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int col, const QModelIndex &parent)
{
    // Dont allow drag'n' drop of complete playlists...
    if (data->property(constPlaylistProp).isValid()) {
        return false;
    }

    // We may have dropped onto a song, so use songs row number...
    int origRow=row;
    if (row<0) {
        row=parent.row();
    }

    if (origRow<0 && dropAdjust) { // Unlike the playqueue, we 'copy' items, so need to adjust 1 more place...
        dropAdjust++;
    }
    row+=dropAdjust;

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
            emit addToPlaylist(pl->name, filenames,
                               row < 0 || (origRow<0 && item->isPlaylist()) ? 0 : row,
                               row < 0 || (origRow<0 && item->isPlaylist()) ? 0 : pl->songs.size());
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
                emit moveInPlaylist(pl->name, PlayQueueModel::decodeInts(*data, constPositionsMimeType), row < 0 ? pl->songs.size() : row, pl->songs.size());
                return true;
            } else {
                emit addToPlaylist(pl->name, filenames,
                                   row < 0 || (origRow<0 && item->isPlaylist()) ? 0 : row,
                                   row < 0 || (origRow<0 && item->isPlaylist()) ? 0 : pl->songs.size());
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
#endif

void PlaylistsModel::getPlaylists()
{
// ALWAYS need to send listPlaylists() -> as it is now used for favourite streams!
//    if (enabled) {
        emit listPlaylists();
//    }
}

void PlaylistsModel::clear()
{
    beginResetModel();
    clearPlaylists();
    updateItemMenu();
    endResetModel();
}

void PlaylistsModel::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    enabled=e;
    if (enabled) {
        getPlaylists();
    } else {
        clear();
    }
}

#ifndef ENABLE_UBUNTU
QMenu * PlaylistsModel::menu()
{
    if (!itemMenu) {
        updateItemMenu(true);
    }
    return itemMenu;
}
#endif

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    if (!enabled) {
        return;
    }

    if (items.isEmpty()) {
        if (playlists.isEmpty()) {
            return;
        }
        beginResetModel();
        foreach (const Playlist &p, playlists) {
            if (p.name!=StreamsModel::constPlayListName) {
                items.append(new PlaylistItem(p, allocateKey()));
            }
        }
        endResetModel();
        updateItemMenu();
        #ifdef ENABLE_UBUNTU
        emit updated();
        #endif
    } else if (playlists.isEmpty()) {
        clear();
        #ifdef ENABLE_UBUNTU
        emit updated();
        #endif
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
            if (p.name==StreamsModel::constPlayListName) {
                continue;
            }
            retreived.insert(p.name);
            PlaylistItem *pl=getPlaylist(p.name);

            if (pl && pl->lastModified<p.lastModified) {
                pl->lastModified=p.lastModified;
                if (pl->loaded && !pl->isSmartPlaylist) {
                    emit playlistInfo(pl->name);
                }
            }
        }

        removed=existing-retreived;
        added=retreived-existing;

        if (removed.count()) {
            foreach (const QString &p, removed) {
                PlaylistItem *pl=getPlaylist(p);
                if (pl) {
                    int index=items.indexOf(pl);
                    beginRemoveRows(parent, index, index);
                    usedKeys.remove(pl->key);
                    emit playlistRemoved(pl->key);
                    delete items.takeAt(index);
                    endRemoveRows();
                }
            }
        }
        if (added.count()) {
            beginInsertRows(parent, items.count(), items.count()+added.count()-1);
            foreach (const QString &p, added) {
                int idx=playlists.indexOf(Playlist(p));
                if (-1!=idx) {
                    items.append(new PlaylistItem(playlists.at(idx), allocateKey()));
                }
            }
            endInsertRows();
        }

        if (added.count() || removed.count()) {
            updateItemMenu();
            #ifdef ENABLE_UBUNTU
            emit updated();
            #endif
        }
    }
}

void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    PlaylistItem *pl=getPlaylist(name);

    if (pl) {
        QModelIndex idx=createIndex(items.indexOf(pl), 0, pl);
        if (pl->songs.isEmpty()) {
            if (songs.isEmpty()) {
                return;
            }
            beginInsertRows(idx, 0, songs.count()-1);
            foreach (const Song &s, songs) {
                pl->songs.append(new SongItem(s, pl));
            }
            endInsertRows();
        } else if (songs.isEmpty()) {
            beginRemoveRows(idx, 0, pl->songs.count()-1);
            pl->clearSongs();
            endRemoveRows();
        } else {
            for (qint32 i=0; i<songs.count(); ++i) {
                Song s=songs.at(i);
                SongItem *si=i<pl->songs.count() ? pl->songs.at(i) : 0;
                if (i>=pl->songs.count() || !(s==*static_cast<Song *>(si))) {
                    si=i<pl->songs.count() ? pl->getSong(s, i) : 0;
                    if (!si) {
                        beginInsertRows(idx, i, i);
                        pl->songs.insert(i, new SongItem(s, pl));
                        endInsertRows();
                    } else {
                        int existing=pl->songs.indexOf(si);
                        beginMoveRows(idx, existing, existing, idx, i>existing ? i+1 : i);
                        SongItem *si=pl->songs.takeAt(existing);
                        si->key=s.key;
                        pl->songs.insert(i, si);
                        endMoveRows();
                    }
                } else if (si) {
                    si->key=s.key;
                }
            }

            if (pl->songs.count()>songs.count()) {
                int toRemove=pl->songs.count()-songs.count();
                beginRemoveRows(idx, pl->songs.count()-toRemove, pl->songs.count()-1);
                for (int i=0; i<toRemove; ++i) {
                    delete pl->songs.takeLast();
                }
                endRemoveRows();
            }
        }
        pl->updateGenres();
        emit updated(idx);
        emit dataChanged(idx, idx);
    } else {
        emit listPlaylists();
    }
    updateGenreList();
}

void PlaylistsModel::removedFromPlaylist(const QString &name, const QList<quint32> &positions)
{
    PlaylistItem *pl=0;
    if (0==positions.count() || !(pl=getPlaylist(name))) {
        emit listPlaylists();
        return;
    }

    quint32 adjust=0;
    QModelIndex parent=createIndex(items.indexOf(pl), 0, pl);
    QList<quint32>::ConstIterator it=positions.constBegin();
    QList<quint32>::ConstIterator end=positions.constEnd();
    while(it!=end) {
        quint32 rowBegin=*it;
        quint32 rowEnd=*it;
        QList<quint32>::ConstIterator next=it+1;
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
        for (quint32 i=rowBegin; i<=rowEnd; ++i) {
            delete pl->songs.takeAt(rowBegin-adjust);
        }
        adjust+=(rowEnd-rowBegin)+1;
        endRemoveRows();
        it++;
        pl->updateGenres();
    }
    emit updated(parent);
    updateGenreList();
}

void PlaylistsModel::movedInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos)
{
    PlaylistItem *pl=0;
    if (!(pl=getPlaylist(name)) || idx.count()>pl->songs.count()) {
        emit listPlaylists();
        return;
    }

    foreach (quint32 i, idx) {
        if (i>=(quint32)pl->songs.count()) {
            emit listPlaylists();
            return;
        }
    }

    QList<quint32> indexes=idx;
    QModelIndex parent=createIndex(items.indexOf(pl), 0, pl);
    while (indexes.count()) {
        quint32 from=indexes.takeLast();
        if (from!=pos) {
            beginMoveRows(parent, from, from, parent, pos>from && pos<(quint32)pl->songs.count() ? pos+1 : pos);
            SongItem *si=pl->songs.takeAt(from);
            pl->songs.insert(pos, si);
            endMoveRows();
        }
    }
    emit updated(parent);
}

void PlaylistsModel::emitAddToExisting()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        emit addToExisting(Utils::strippedText(act->text()));
    }
}

void PlaylistsModel::playlistRenamed(const QString &from, const QString &to)
{
    PlaylistItem *pl=getPlaylist(from);

    if (pl) {
        pl->name=to;
        updateItemMenu();
    }
}

void PlaylistsModel::mpdConnectionStateChanged(bool connected)
{
    if (!connected) {
        clear();
    } else {
        getPlaylists();
    }
}

void PlaylistsModel::updateItemMenu(bool create)
{
    #ifndef ENABLE_UBUNTU
    if (!itemMenu) {
        if (!create) {
            return;
        }
        itemMenu = new QMenu(0);
    }

    itemMenu->clear();
    itemMenu->addAction(newAction);
    QStringList names;
    foreach (const PlaylistItem *p, items) {
        if (!p->isSmartPlaylist) {
            names << p->name;
        }
    }
    qSort(names.begin(), names.end(), PlaylistsProxyModel::compareNames);
    foreach (const QString &n, names) {
        itemMenu->addAction(n, this, SLOT(emitAddToExisting()));
    }
    #endif
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
    foreach (PlaylistItem *p, items) {
        usedKeys.remove(p->key);
        emit playlistRemoved(p->key);
    }

    qDeleteAll(items);
    items.clear();
    updateGenreList();
}

void PlaylistsModel::updateGenreList()
{
    QSet<QString> newGenres;
    foreach (PlaylistItem *p, items) {
        newGenres+=p->genres;
    }

    if (newGenres!=plGenres) {
        plGenres=newGenres;
        emit updateGenres(plGenres);
    }
}

quint32 PlaylistsModel::allocateKey()
{
    for(quint32 k=1; k<0xFFFFFFFE; ++k) {
        if (!usedKeys.contains(k)) {
            usedKeys.insert(k);
            return k;
        }
    }

    return 0xFFFFFFFF;
}

PlaylistsModel::PlaylistItem::PlaylistItem(const Playlist &pl, quint32 k)
    : name(pl.name)
    , time(0)
    , key(k)
    , lastModified(pl.lastModified)
{
    loaded=isSmartPlaylist=MPDConnection::self()->isMopdidy() && name.startsWith("Smart Playlist:");
    if (isSmartPlaylist) {
        shortName=name.mid(16);
    }
}

PlaylistsModel::PlaylistItem::~PlaylistItem()
{
    clearSongs();
}

void PlaylistsModel::PlaylistItem::updateGenres()
{
    genres.clear();
    foreach (const SongItem *s, songs) {
        if (!s->genre.isEmpty()) {
            genres.insert(s->genre);
        }
    }
}

void PlaylistsModel::PlaylistItem::clearSongs()
{
    qDeleteAll(songs);
    songs.clear();
    updateGenres();
}

PlaylistsModel::SongItem * PlaylistsModel::PlaylistItem::getSong(const Song &song, int offset)
{
    int i=0;
    foreach (SongItem *s, songs) {
        if (++i<offset) {
            continue;
        }
        if (*s==song) {
            return s;
        }
    }

    return 0;
}

quint32 PlaylistsModel::PlaylistItem::totalTime()
{
    if (0==time) {
        foreach (SongItem *s, songs) {
            time+=s->time;
        }
    }
    return time;
}
