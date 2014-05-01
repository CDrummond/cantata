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

#ifndef MUSIC_LIBRARY_ITEM_SONG_H
#define MUSIC_LIBRARY_ITEM_SONG_H

#include <QList>
#include <QVariant>
#include "musiclibraryitem.h"
#include "song.h"

class MusicLibraryItemAlbum;

class MusicLibraryItemSong : public MusicLibraryItem
{
public:
    MusicLibraryItemSong(const Song &s, MusicLibraryItemContainer *parent)
        : MusicLibraryItem(s.displayTitle(), parent), m_song(s) { }

    virtual ~MusicLibraryItemSong() { }

    const QString & file() const { return m_song.file; }
    void setSong(const Song &s) { m_song=s; m_genres.clear(); }
    void setFile(const QString &f) { m_song.file=f; }
    quint16 track() const { return m_song.track; }
    void setTrack(quint16 t) { m_song.track=t; }
    void setPlayed(bool p) { m_song.setPlayed(p); }
    quint16 disc() const { return m_song.disc; }
    quint32 time() const { return m_song.time; }
    const QString & genre() const { return m_song.genre; }
    const Song & song() const { return m_song; }
    Type itemType() const { return Type_Song; }
    bool hasGenre(const QString &genre) const { initGenres(); return m_genres.contains(genre); }
    const QSet<QString> & allGenres() const { initGenres(); return m_genres; }
    void setPodcastImage(const QString &img) { m_song.setPodcastImage(img); }

private:
    void initGenres() const;

protected:
    Song m_song;
    mutable QSet<QString> m_genres;
};

#endif
