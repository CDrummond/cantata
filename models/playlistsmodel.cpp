/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QFont>
#include "config.h"
#include "playlistsmodel.h"
#include "playlistsproxymodel.h"
#include "playqueuemodel.h"
#include "widgets/groupedview.h"
#include "roles.h"
#include "gui/covers.h"
#include "gui/settings.h"
#include "support/utils.h"
#include "support/monoicon.h"
#include "support/globalstatic.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdstats.h"
#include "mpd-interface/mpdconnection.h"
#include "playqueuemodel.h"
#include "widgets/icons.h"
#include "widgets/mirrormenu.h"
#ifdef ENABLE_HTTP_SERVER
#include "http/httpserver.h"
#endif
#include <algorithm>

QString PlaylistsModel::headerText(int col)
{
    switch (col) {
    case COL_TITLE:     return PlayQueueModel::headerText(PlayQueueModel::COL_TITLE);
    case COL_ARTIST:    return PlayQueueModel::headerText(PlayQueueModel::COL_ARTIST);
    case COL_ALBUM:     return PlayQueueModel::headerText(PlayQueueModel::COL_ALBUM);
    case COL_LENGTH:    return PlayQueueModel::headerText(PlayQueueModel::COL_LENGTH);
    case COL_YEAR:      return PlayQueueModel::headerText(PlayQueueModel::COL_YEAR);
    case COL_ORIGYEAR:  return PlayQueueModel::headerText(PlayQueueModel::COL_ORIGYEAR);
    case COL_GENRE:     return PlayQueueModel::headerText(PlayQueueModel::COL_GENRE);
    case COL_COMPOSER:  return PlayQueueModel::headerText(PlayQueueModel::COL_COMPOSER);
    case COL_PERFORMER: return PlayQueueModel::headerText(PlayQueueModel::COL_PERFORMER);
    case COL_FILENAME:  return PlayQueueModel::headerText(PlayQueueModel::COL_FILENAME);
    case COL_PATH:      return PlayQueueModel::headerText(PlayQueueModel::COL_PATH);
    default:            return QString();
    }
}

GLOBAL_STATIC(PlaylistsModel, instance)

PlaylistsModel::PlaylistsModel(QObject *parent)
    : ActionModel(parent)
    , multiCol(false)
    , itemMenu(nullptr)
    , dropAdjust(0)
{
    icn=Icons::self()->playlistListIcon;
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
    connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
    newAction=new QAction(MonoIcon::icon(FontAwesome::asterisk, Utils::monoIconColor()), tr("New Playlist..."), this);
    connect(newAction, SIGNAL(triggered()), this, SIGNAL(addToNew()));
    Action::initIcon(newAction);
    alignments[COL_TITLE]=alignments[COL_ARTIST]=alignments[COL_ALBUM]=alignments[COL_GENRE]=alignments[COL_COMPOSER]=
            alignments[COL_PERFORMER]=alignments[COL_FILENAME]=alignments[COL_PATH]=int(Qt::AlignVCenter|Qt::AlignLeft);
    alignments[COL_LENGTH]=alignments[COL_YEAR]=alignments[COL_ORIGYEAR]=int(Qt::AlignVCenter|Qt::AlignRight);
}

PlaylistsModel::~PlaylistsModel()
{
    if (itemMenu) {
        itemMenu->deleteLater();
        itemMenu=nullptr;
    }
}

QString PlaylistsModel::name() const
{
    return QLatin1String("playlists");
}

QString PlaylistsModel::title() const
{
    return tr("Stored Playlists");
}

QString PlaylistsModel::descr() const
{
    return tr("Standard playlists");
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

QVariant PlaylistsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal==orientation) {
        switch (role) {
        case Qt::DisplayRole:
        case Cantata::Role_ContextMenuText:
            return headerText(section);
        case Qt::TextAlignmentRole:
            return alignments[section];
        case Cantata::Role_InitiallyHidden:
            return COL_YEAR==section || COL_ORIGYEAR==section || COL_GENRE==section || COL_COMPOSER==section || COL_PERFORMER==section ||
                   COL_FILENAME==section || COL_PATH==section;
        case Cantata::Role_Hideable:
            return COL_LENGTH!=section;
        case Cantata::Role_Width:
            switch (section) {
            case COL_TITLE:     return 0.4;
            case COL_ARTIST:    return 0.15;
            case COL_ALBUM:     return 0.15;
            case COL_YEAR:      return 0.05;
            case COL_ORIGYEAR:  return 0.05;
            case COL_GENRE:     return 0.125;
            case COL_LENGTH:    return 0.125;
            case COL_COMPOSER:  return 0.15;
            case COL_PERFORMER: return 0.15;
            case COL_FILENAME:  return 0.15;
            case COL_PATH:      return 0.15;
            }
        default:
            break;
        }
    }

    return QVariant();
}

bool PlaylistsModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (Qt::Horizontal==orientation && Qt::TextAlignmentRole==role && section>=0) {
        int al=value.toInt()|Qt::AlignVCenter;
        if (al!=alignments[section]) {
            alignments[section]=al;
            return true;
        }
    }
    return false;
}

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
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

    if (Qt::TextAlignmentRole==role) {
        return alignments[index.column()];
    }

    Item *item=static_cast<Item *>(index.internalPointer());

    if (item->isPlaylist()) {
        PlaylistItem *pl=static_cast<PlaylistItem *>(item);

        switch(role) {
        case Cantata::Role_ListImage:
            return false;
        case Cantata::Role_IsCollection:
            return true;
        case Cantata::Role_CollectionId:
            return pl->key;
        case Cantata::Role_Key:
            return 0;
        case Cantata::Role_SongWithRating:
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
                case COL_ORIGYEAR:
                case COL_GENRE:
                    return QVariant();
                default:
                    break;
                }
            }
            return pl->visibleName();
        case Qt::ToolTipRole:
            if (!Settings::self()->infoTooltips()) {
                return QVariant();
            }
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            return 0==pl->songs.count()
                ? pl->visibleName()
                : pl->visibleName()+"\n"+tr("%n Tracks (%1)", "", pl->songs.count()).arg(Utils::formatTime(pl->totalTime()));
        case Qt::DecorationRole:
            return multiCol ? QVariant() : (pl->isSmartPlaylist ? Icons::self()->smartPlaylistIcon : Icons::self()->playlistListIcon);
        case Cantata::Role_SubText:
            if (!pl->loaded) {
                pl->loaded=true;
                emit playlistInfo(pl->name);
            }
            if (pl->isSmartPlaylist) {
                return tr("Smart Playlist");
            }
            return tr("%n Tracks (%1)", "", pl->songs.count()).arg(Utils::formatTime(pl->totalTime()));
        case Cantata::Role_TitleActions:
            return true;
        default:
            return ActionModel::data(index, role);
        }
    } else {
        SongItem *s=static_cast<SongItem *>(item);

        switch (role) {
        case Cantata::Role_ListImage:
            return true;
        case Cantata::Role_IsCollection:
            return false;
        case Cantata::Role_CollectionId:
            return s->parent->key;
        case Cantata::Role_Key:
            return s->key;
        case Cantata::Role_CoverSong:
        case Cantata::Role_SongWithRating:
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
                    if (s->isStream() && s->album.isEmpty()) {
                        QString n=s->name();
                        if (!n.isEmpty()) {
                            return n;
                        }
                    }
                    return s->album;
                case COL_LENGTH:
                    return Utils::formatTime(s->time);
                case COL_YEAR:
                    if (s->year <= 0) {
                        return QVariant();
                    }
                    return s->year;
                case COL_ORIGYEAR:
                    if (s->origYear <= 0) {
                        return QVariant();
                    }
                    return s->origYear;
                case COL_GENRE:
                    return s->displayGenre();
                case COL_COMPOSER:
                    return s->composer();
                case COL_PERFORMER:
                    return s->performer();
                case COL_FILENAME:
                    return Utils::getFile(QUrl(s->file).path());
                case COL_PATH: {
                    QString dir=Utils::getDir(QUrl(s->file).path());
                    return dir.endsWith("/") ? dir.left(dir.length()-1) : dir;
                }
                default:
                    break;
                }
            }
            return s->entryName();
        case Qt::ToolTipRole:
            if (!Settings::self()->infoTooltips()) {
                return QVariant();
            }
            return s->toolTip();
        case Qt::DecorationRole:
            return multiCol ? QVariant() : (s->title.isEmpty() ? Icons::self()->streamIcon : Icons::self()->audioListIcon);
        case Cantata::Role_MainText:
            return s->title.isEmpty() ? s->name().isEmpty() ? s->file : s->name() : s->title;
        case Cantata::Role_SubText:
            return (s->artist.isEmpty() ? QString() : (s->artist+Song::constSep))+
                   (s->displayAlbum().isEmpty() ? QString() : (s->displayAlbum()+Song::constSep))+
                   (s->time>0 ? Utils::formatTime(s->time) : QString());
        case Cantata::Role_TextColor:
            if (s->isInvalid()) {
                return MonoIcon::constRed;
            }
        default:
            return ActionModel::data(index, role);
        }
    }

    return QVariant();
}

bool PlaylistsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Cantata::Role_DropAdjust==role) {
        dropAdjust=value.toUInt();
        return true;
    } else {
        return QAbstractItemModel::setData(index, value, role);
    }
}

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

static QString songFileName(const Song &song)
{
    if (song.isStream() && song.title.isEmpty()) {
        return MPDParseUtils::addStreamName(song.file, song.name());
    }
    return song.file;
}

QStringList PlaylistsModel::filenames(const QModelIndexList &indexes, bool filesOnly) const
{
    QStringList fnames;
    QSet<Item *> selectedPlaylists;
    for(QModelIndex index: indexes) {
        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            selectedPlaylists.insert(item);
            for (const SongItem *s: static_cast<PlaylistItem*>(item)->songs) {
                if (!filesOnly || !s->file.contains(':')) {
                    fnames << s->file;
                }
            }
        } else if (!selectedPlaylists.contains(static_cast<SongItem*>(item)->parent)) {
            if (!filesOnly || !static_cast<SongItem*>(item)->file.contains(':')) {
                fnames << songFileName(* static_cast<SongItem*>(item));
            }
        }
    }

    return fnames;
}

QList<Song> PlaylistsModel::songs(const QModelIndexList &indexes) const
{
    QList<Song> songs;
    QSet<Item *> selectedPlaylists;

    for (const QModelIndex &idx: indexes) {
        Item *item=static_cast<Item *>(idx.internalPointer());
        if (item->isPlaylist()) {
            PlaylistItem *playlist=static_cast<PlaylistItem *>(item);
            for (const SongItem *song: playlist->songs) {
                selectedPlaylists.insert(item);
                songs.append(*song);
            }
        }
    }
    for (const QModelIndex &idx: indexes) {
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
    for (QModelIndex index: indexes) {
        if (!index.isValid() || 0!=index.column()) {
            continue;
        }

        Item *item=static_cast<Item *>(index.internalPointer());

        if (item->isPlaylist()) {
            PlaylistItem *playlist=static_cast<PlaylistItem*>(item);
            int pos=0;
            selectedPlaylists.insert(item);
            if (playlist->songs.isEmpty()) {
                filenames << MPDConnection::constPlaylistPrefix+playlist->name;
                playlists << playlist->name;
                positions << pos++;
            } else {
                for (const SongItem *s: playlist->songs) {
                    filenames << s->file;
                    playlists << playlist->name;
                    positions << pos++;
               }
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
            for (const QString &p: playlists) {
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

MirrorMenu * PlaylistsModel::menu()
{
    if (!itemMenu) {
        updateItemMenu(true);
    }
    return itemMenu;
}

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    if (items.isEmpty()) {
        if (playlists.isEmpty()) {
            return;
        }
        beginResetModel();
        for (const Playlist &p: playlists) {
            items.append(new PlaylistItem(p, allocateKey()));
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

        for (PlaylistItem *p: items) {
            existing.insert(p->name);
        }

        for (const Playlist &p: playlists) {
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
            for (const QString &p: removed) {
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
            for (const QString &p: added) {
                int idx=playlists.indexOf(Playlist(p));
                if (-1!=idx) {
                    items.append(new PlaylistItem(playlists.at(idx), allocateKey()));
                }
            }
            endInsertRows();
        }

        if (added.count() || removed.count()) {
            updateItemMenu();
        }
    }
}

void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    PlaylistItem *pl=getPlaylist(name);

    if (pl) {
        QModelIndex idx=createIndex(items.indexOf(pl), 0, pl);

        // If Cantata moves songs, then movedInPlaylist() is called. Likwise if
        // Cantata deltes songs, removedFromPlaylist() is called. Therefore, no
        // need to do partial updates here. If another client does a move whilst
        // a playlist has duplicate songs, then it can fail. #873
        if (!pl->songs.isEmpty()) {
            beginRemoveRows(idx, 0, pl->songs.count()-1);
            pl->clearSongs();
            endRemoveRows();
        }

        if (!songs.isEmpty()) {
            beginInsertRows(idx, 0, songs.count()-1);
            for (const Song &s: songs) {
                pl->songs.append(new SongItem(s, pl));
            }
            endInsertRows();
        }

        emit updated(idx);
        emit dataChanged(idx, idx);
    } else {
        emit listPlaylists();
    }
}

void PlaylistsModel::removedFromPlaylist(const QString &name, const QList<quint32> &positions)
{
    PlaylistItem *pl=nullptr;
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
    }
    emit updated(parent);
}

void PlaylistsModel::movedInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos)
{
    PlaylistItem *pl=nullptr;
    if (!(pl=getPlaylist(name)) || idx.count()>pl->songs.count()) {
        emit listPlaylists();
        return;
    }

    for (quint32 i: idx) {
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

void PlaylistsModel::coverLoaded(const Song &song, int s)
{
    Q_UNUSED(s)
    if (!song.isArtistImageRequest() && !song.isComposerImageRequest()) {
        int plRow=0;
        for (const PlaylistItem *pl: items) {
            QModelIndex plIdx;
            int songRow=0;
            for (const SongItem *s: pl->songs) {
                if (s->albumArtist()==song.albumArtist() && s->album==song.album) {
                    if (!plIdx.isValid()) {
                        plIdx=index(plRow, 0, QModelIndex());
                    }
                    QModelIndex idx=index(songRow, 0, plIdx);
                    emit dataChanged(idx, idx);
                }
                songRow++;
            }
            plRow++;
        }
    }
}

void PlaylistsModel::updateItemMenu(bool create)
{
    if (!itemMenu) {
        if (!create) {
            return;
        }
        itemMenu = new MirrorMenu(nullptr);
    }

    itemMenu->clear();
    itemMenu->addAction(newAction);
    QStringList names;
    for (const PlaylistItem *p: items) {
        if (!p->isSmartPlaylist) {
            names << p->name;
        }
    }
    std::sort(names.begin(), names.end(), PlaylistsProxyModel::compareNames);
    for (const QString &n: names) {
        itemMenu->addAction(n, this, SLOT(emitAddToExisting()));
    }
}

PlaylistsModel::PlaylistItem * PlaylistsModel::getPlaylist(const QString &name)
{
    for (PlaylistItem *p: items) {
        if (p->name==name) {
            return p;
        }
    }

    return nullptr;
}

void PlaylistsModel::clearPlaylists()
{
    for (PlaylistItem *p: items) {
        usedKeys.remove(p->key);
        emit playlistRemoved(p->key);
    }

    qDeleteAll(items);
    items.clear();
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
    loaded=isSmartPlaylist=MPDConnection::self()->isMopidy() && name.startsWith("Smart Playlist:");
    if (isSmartPlaylist) {
        shortName=name.mid(16);
    }
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

PlaylistsModel::SongItem * PlaylistsModel::PlaylistItem::getSong(const Song &song, int offset)
{
    int i=0;
    for (SongItem *s: songs) {
        if (++i<offset) {
            continue;
        }
        if (*s==song) {
            return s;
        }
    }

    return nullptr;
}

quint32 PlaylistsModel::PlaylistItem::totalTime()
{
    if (0==time) {
        for (SongItem *s: songs) {
            time+=s->time;
        }
    }
    return time;
}

#include "moc_playlistsmodel.cpp"
