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

#ifndef MUSIC_LIBRARY_ITEM_ARTIST_H
#define MUSIC_LIBRARY_ITEM_ARTIST_H

#include <QList>
#include <QVariant>
#include <QHash>
#include "musiclibraryitem.h"

class Song;
class MusicLibraryItemRoot;
class MusicLibraryItemAlbum;

class MusicLibraryItemArtist : public MusicLibraryItem
{
public:
    MusicLibraryItemArtist(const QString &data, MusicLibraryItem *parent = 0);
    ~MusicLibraryItemArtist();

    MusicLibraryItemAlbum * album(const Song &s);
    MusicLibraryItem * child(int row) const;
    int childCount() const;
    int row() const;
    MusicLibraryItem * parent() const;
    void setParent(MusicLibraryItem * const parent);
    const QString & baseArtist() const;

private:
    QString nonTheArtist;
    QHash<QString, int> m_indexes;
    QList<MusicLibraryItemAlbum *> m_childItems;
    MusicLibraryItemRoot *m_parentItem;

    friend class MusicLibraryItemAlbum;
};

#endif
