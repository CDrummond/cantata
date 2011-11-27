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

#ifndef PLAYLISTS_MODEL_H
#define PLAYLISTS_MODEL_H

#include <QAbstractListModel>
#include <QList>
#include "lib/playlist.h"

class PlaylistsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    PlaylistsModel(QObject *parent = 0);
    ~PlaylistsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &, int) const;

    void getPlaylists();
    void renamePlaylist(const QString oldname, const QString newname);

public slots:
    void loadPlaylist(QString name);
    void removePlaylist(QString name);
    void savePlaylist(QString name);

private:
    QList<Playlist> m_playlists;

private slots:
    void setPlaylists(const QList<Playlist> &playlists);

signals:
    void playlistLoaded();
};

#endif
