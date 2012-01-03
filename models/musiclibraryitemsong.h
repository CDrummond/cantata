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

#ifndef MUSIC_LIBRARY_ITEM_SONG_H
#define MUSIC_LIBRARY_ITEM_SONG_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include "musiclibraryitem.h"
#include "song.h"

class MusicLibraryItemAlbum;

class MusicLibraryItemSong : public MusicLibraryItem
{
public:
    MusicLibraryItemSong(const Song &s, MusicLibraryItem *parent)
        : MusicLibraryItem(s.displayTitle(), MusicLibraryItem::Type_Song, parent)
        , m_song(s) {
    }

    virtual ~MusicLibraryItemSong() {
    }

    const QString & file() const {
        return m_song.file;
    }
    quint32 track() const {
        return m_song.track;
    }
    quint32 disc() const {
        return m_song.disc;
    }
    quint32 time() const {
        return m_song.time;
    }
    QString genre() const {
        return m_genres.isEmpty() ? QString() : *m_genres.constBegin();
    }
    const Song & song() const {
        return m_song;
    }
private:
    Song m_song;
};

#endif
