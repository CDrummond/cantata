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

#include <QtCore/QModelIndex>
#include <QtGui/QMenu>
#include "playlistsmodel.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(PlaylistsModel, instance)
#endif
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdconnection.h"

PlaylistsModel * PlaylistsModel::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static PlaylistsModel *instance=0;;
    if(!instance) {
        instance=new Covers;
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
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(const QString &)), MPDConnection::self(), SLOT(playlistInfo(const QString &)));
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

    if(item->isPlaylist())
        return QModelIndex();
    else
    {
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
            return pl->name;
        case Qt::ToolTipRole:
            return 0==pl->songs.count()
                ? pl->name
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Song", "%1\n%2 Songs", pl->name, pl->songs.count());
                    #else
                    (pl->songs.count()>1
                        ? tr("%1\n%2 Songs").arg(pl->name).arg(pl->songs.count())
                        : tr("%1\n1 Song").arg(pl->name);
                    #endif
        default: break;
        }
    } else {
        SongItem *s=static_cast<SongItem *>(item);

        if (Qt::DisplayRole==role || Qt::ToolTipRole==role) {
            QString text=s->title.isEmpty() ? s->file : s->title;

            if (!s->title.isEmpty()) {
                text=s->artist+QLatin1String(" - ")+text;
                if (Qt::ToolTipRole==role) {
                    QString duration=MPDParseUtils::formatDuration(s->time);
                    if (duration.startsWith(QLatin1String("00:"))) {
                        duration=duration.mid(3);
                    }
                    if (duration.startsWith(QLatin1String("00:"))) {
                        duration=duration.mid(1);
                    }
                    text=text+QChar('\n')+duration;
                }
            }
            return text;
        }
    }

    return QVariant();
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
    bool diff=playlists.count()!=items.count();

    if (!diff) {
        foreach (const Playlist &p, playlists) {
            if (!getPlaylist(p.name)) {
                diff=true;
                break;
            }
        }
    }
    if (diff) {
        beginResetModel();
        clearPlaylists();
        foreach (const Playlist &p, playlists) {
            items.append(new PlaylistItem(p));
        }
        updateItemMenu();
    }
    if (diff) {
        endResetModel();
    }
}

void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    PlaylistItem *pl=getPlaylist(name);

    if (pl) {
        bool diff=songs.count()!=pl->songs.count();

        if (!diff) {
            foreach (const Song &s, songs) {
                if (!pl->getSong(s)) {
                    diff=true;
                    break;
                }
            }
        }

        if (diff) {
            beginResetModel();
            pl->clearSongs();
            foreach (const Song &s, songs) {
                pl->songs.append(new SongItem(s, pl));
            }
            endResetModel();
        }
    } else {
        emit listPlaylists();
    }
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
