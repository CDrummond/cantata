/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SEARCH_MODEL_H
#define SEARCH_MODEL_H

#include "mpd/song.h"
#include "actionmodel.h"
#include <QList>
#include <QMap>

class SearchModel : public ActionModel
{
    Q_OBJECT

public:

    enum Columns
    {
        COL_TRACK,
        COL_DISC,
        COL_TITLE,
        COL_ARTIST,
        COL_ALBUM,
        COL_LENGTH,
        COL_YEAR,
        COL_GENRE,
        COL_COMPOSER,
        COL_PERFORMER,
        COL_RATING,

        COL_COUNT
    };

    static QString headerText(int col);

    SearchModel(QObject *parent = 0);
    ~SearchModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    #ifndef ENABLE_UBUNTU
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole);
    #endif
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const { return COL_COUNT; }
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    #ifndef ENABLE_UBUNTU
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;
    #endif

    void refresh();
    void clear();
    void search(const QString &key, const QString &value);
    void setMultiColumn(bool m) { multiCol=m; }

Q_SIGNALS:
    void searching();
    void searched();
    void statsUpdated(int songs, quint32 time);

    void search(const QString &field, const QString &value, int id);
    void getRating(const QString &file) const;

private Q_SLOTS:
    void searchFinished(int id, const QList<Song> &result);
    void coverLoaded(const Song &song, int s);
    void ratingResult(const QString &file, quint8 r);

private:
    void clearItems();
    const Song * toSong(const QModelIndex &index) const { return index.isValid() ? static_cast<const Song *>(index.internalPointer()) : 0; }

private:
    bool multiCol;
    QList<Song> songList;
    int currentId;
    QString currentKey;
    QString currentValue;
    #ifndef ENABLE_UBUNTU
    QMap<int, int> alignments;
    #endif
};

#endif
