/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "mpd-interface/song.h"
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
        COL_ORIGYEAR,
        COL_GENRE,
        COL_COMPOSER,
        COL_PERFORMER,
        COL_RATING,

        COL_COUNT
    };

    static QString headerText(int col);

    SearchModel(QObject *parent = nullptr);
    ~SearchModel() override;
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override { return COL_COUNT; }
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    void refresh();
    virtual void clear();
    virtual void search(const QString &key, const QString &value)=0;
    void setMultiColumn(bool m) { multiCol=m; }

Q_SIGNALS:
    void searching();
    void searched();
    void statsUpdated(int songs, quint32 time);

protected:
    virtual Song & fixPath(Song &s) const { return s; }
    void results(const QList<Song> &songs);
    const Song * toSong(const QModelIndex &index) const { return index.isValid() ? static_cast<const Song *>(index.internalPointer()) : nullptr; }

protected:
    bool multiCol;
    QList<Song> songList;
    QString currentKey;
    QString currentValue;
    QMap<int, int> alignments;
};

#endif
