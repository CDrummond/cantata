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

#ifndef MUSIC_LIBRARY_MODEL_H
#define MUSIC_LIBRARY_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QDateTime>
#include "musiclibraryitemroot.h"
#include "song.h"

class QMimeData;
class MusicLibraryItemAlbum;
class MusicLibraryItemArtist;

class MusicLibraryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    MusicLibraryModel(QObject *parent = 0);
    ~MusicLibraryModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes) const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    const MusicLibraryItemRoot * root() const { return rootItem; }
    bool isFromSingleTracks(const Song &s) const { return rootItem->isFromSingleTracks(s); }

    bool fromXML(const QDateTime db_update);
    void clearUpdateTime() { databaseTime=QDateTime(); }
    void clear();

public Q_SLOTS:
    void updateMusicLibrary(MusicLibraryItemRoot * root, QDateTime db_update = QDateTime(), bool fromFile = false);
    void setCover(const QString &artist, const QString &album, const QImage &img, const QString &file);

Q_SIGNALS:
    void xmlWritten(QDateTime db_update);
    void updated(const MusicLibraryItemRoot *root);
    void updateGenres(const QStringList &genres);

private:
    const MusicLibraryItemRoot * rootItem;

    void toXML(const QDateTime db_update);
    QDateTime databaseTime;
};

#endif
