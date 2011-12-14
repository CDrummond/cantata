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

#ifndef PLAYLISTS_MODEL_H
#define PLAYLISTS_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include "playlist.h"

class QMenu;

class PlaylistsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static PlaylistsModel * self();

    PlaylistsModel(QObject *parent = 0);
    ~PlaylistsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int col, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    void getPlaylists();
    void clear();
    bool exists(const QString &n) const;

    QMenu * menu() { return itemMenu; }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void listPlaylists();
    void playlistInfo(const QString &name);

    void addToNew();
    void addToExisting(const QString &name);

private Q_SLOTS:
    void setPlaylists(const QList<Playlist> &playlists);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void emitAddToExisting();

private:
    void updateItemMenu();

private:
    QList<Playlist> items;
    QMenu *itemMenu;
};

#endif
