/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/song.h"

class MusicLibraryItemRoot;
class MusicLibraryItemAlbum;

class MusicLibraryItemArtist : public MusicLibraryItemContainer
{
public:
    static bool lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b);

    MusicLibraryItemArtist(const Song &song, MusicLibraryItemContainer *parent=nullptr);
    ~MusicLibraryItemArtist() override { }

    MusicLibraryItemAlbum * album(const Song &s, bool create=true);
    MusicLibraryItemAlbum * createAlbum(const Song &s);
    const QString & sortString() const { return m_sortString.isEmpty() ? m_itemData : m_sortString; }
    void remove(MusicLibraryItemAlbum *album);
    Type itemType() const override { return Type_Artist; }
    // 'data' could be 'Composer' if we are set to use that, but need to save real artist...
    const QString & actualArtist() const { return m_actualArtist; }
    Song coverSong() const;

private:
    MusicLibraryItemAlbum * getAlbum(const QString &key) const;

private:
    QString m_sortString; // Do we have an actual artist-sort, or is m_sortString just "Artist, The" ??? - needed for cache saving
    QString m_actualArtist;
    mutable QHash<QString, int> m_indexes;
};

#endif
