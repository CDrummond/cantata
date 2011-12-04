/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef PLAYLIST_TABLE_MODEL_H
#define PLAYLIST_TABLE_MODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QStringList>
#include <QSet>
#include "song.h"

class PlaylistTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    PlaylistTableModel(QObject *parent = 0);
    ~PlaylistTableModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    void updateCurrentSong(quint32 id);
    qint32 getIdByRow(qint32 row) const;
    qint32 getPosByRow(qint32 row) const;
    qint32 getRowById(qint32 id) const;
    Song getSongByRow(const qint32 row) const;

    Qt::DropActions supportedDropActions() const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);

    QSet<qint32> getSongIdSet();

    void playListStats();

public slots:
    void updatePlaylist(const QList<Song> &songs);

signals:
    void filesAddedInPlaylist(const QStringList filenames, const int row, const int size);
    void moveInPlaylist(const QList<quint32> items, const int row, const int size);
    void playListStatsUpdated();

private:
    QList<Song> songs;
    qint32 song_id;

private slots:
    void playListReset();
};

#endif
