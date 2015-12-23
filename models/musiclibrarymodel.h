/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QSet>
#include <QMap>
#include "musiclibraryitemroot.h"
#include "musiclibraryitemalbum.h"
#include "mpd-interface/song.h"
#include "actionmodel.h"

class QMimeData;
class MusicLibraryItemArtist;

class MusicLibraryModel : public ActionModel
{
public:
    MusicLibraryModel(QObject *parent=0);
    ~MusicLibraryModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &i=QModelIndex()) const { Q_UNUSED(i) return 1; }
    QVariant data(const QModelIndex &, int) const;
    bool setData(const QModelIndex &idx, const QVariant &value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex &index) const;
    void clear();
    void setSongs(const QSet<Song> &songs);
    void setSupportsAlbumArtistTag(bool s) { rootItem->setSupportsAlbumArtistTag(s); }
    virtual int row(void *i) const { return rootItem->indexOf(static_cast<MusicLibraryItem *>(i)); }

protected:
    const MusicLibraryItemRoot * root(const MusicLibraryItem *item) const;

private:
    void setParentState(const QModelIndex &parent);

private:
    MusicLibraryItemRoot *rootItem;

    friend class MusicLibraryItemRoot;
    friend class DevicesModel;
    friend class Device;
};

#endif
