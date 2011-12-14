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

#include <QModelIndex>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "playlistsmodel.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdconnection.h"

PlaylistsModel::PlaylistsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(const QList<Playlist> &)), this, SLOT(setPlaylists(const QList<Playlist> &)));
    connect(MPDConnection::self(), SIGNAL(playlistInfoRetrieved(const QString &, const QList<Song> &)), this, SLOT(playlistInfoRetrieved(const QString &, const QList<Song> &)));
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(const QString &)), MPDConnection::self(), SLOT(playlistInfo(const QString &)));
}

PlaylistsModel::~PlaylistsModel()
{
}

QVariant PlaylistsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

// QModelIndex PlaylistsModel::index(int row, int column, const QModelIndex &parent) const
// {
//     Q_UNUSED(parent)
//
//     if(row<items.count())
//         return createIndex(row, column, (void *)&items.at(row));
//
//     return QModelIndex();
// }

int PlaylistsModel::rowCount(const QModelIndex &) const
{
    return items.size();
}

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= items.size())
        return QVariant();

    const Playlist &pl=items.at(index.row());

    switch(role) {
    case Qt::DisplayRole: return pl.name;
    case Qt::ToolTipRole:
        return 0==pl.songs.count()
            ? items.at(index.row()).name
            :
                #ifdef ENABLE_KDE_SUPPORT
                i18np("%1\n1 Song", "%1\n%2 Songs", pl.name, pl.songs.count());
                #else
                (pl.songs.count()>1
                    ? tr("%1\n%2 Songs").arg(pl.name).arg(pl.songs.count())
                    : tr("%1\n1 Song").arg(pl.name);
                #endif
    default: break;
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
    items=QList<Playlist>();
    endResetModel();
}

bool PlaylistsModel::exists(const QString &n) const
{
    foreach (const Playlist &p, items) {
        if (p.name==n) {
            return true;
        }
    }

    return false;
}

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    bool diff=playlists.count()!=items.count();

    if (!diff) {
        foreach (const Playlist &p, playlists) {
            if (!items.contains(p)) {
                diff=true;
                break;
            }
        }
    }
    if (diff) {
        beginResetModel();
        items=playlists;
    }
    foreach (const Playlist &p, items) {
        emit playlistInfo(p.name);
    }
    if (diff) {
        endResetModel();
    }
}
#include <QtCore/QDebug>
void PlaylistsModel::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    int index=items.indexOf(Playlist(name));

    if (index>=0 && index<items.count()) {
        Playlist &pl=items[index];
        bool diff=songs.count()!=pl.songs.count();

        if (!diff) {
            foreach (const Song &s, songs) {
                if (-1!=pl.songs.indexOf(s)) {
                    diff=true;
                    break;
                }
            }
        }

        if (diff) {
//             if (pl.songs.count()) {
//                 beginRemoveRows(index(
//             }
            pl.songs=songs;
        }
    } else {
        emit listPlaylists();
    }
}
