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
#include "playlistsmodel.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdconnection.h"

PlaylistsModel::PlaylistsModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(const QList<Playlist> &)), this, SLOT(setPlaylists(const QList<Playlist> &)));
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
}

PlaylistsModel::~PlaylistsModel()
{
}

QVariant PlaylistsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int PlaylistsModel::rowCount(const QModelIndex &) const
{
    return m_playlists.size();
}

QVariant PlaylistsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_playlists.size())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        return m_playlists.at(index.row()).m_name;

    return QVariant();
}

void PlaylistsModel::getPlaylists()
{
    emit listPlaylists();
}

void PlaylistsModel::clear()
{
    beginResetModel();
    m_playlists=QList<Playlist>();
    endResetModel();
}

bool PlaylistsModel::exists(const QString &n) const
{
    foreach (const Playlist &p, m_playlists) {
        if (p.m_name==n) {
            return true;
        }
    }

    return false;
}

void PlaylistsModel::setPlaylists(const QList<Playlist> &playlists)
{
    beginResetModel();
    m_playlists=playlists;
    endResetModel();
}

