/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "musiclibraryitemsong.h"

MusicLibraryItemSong::~MusicLibraryItemSong()
{
    if (m_genres>(void *)1) {
        delete m_genres;
        m_genres=0;
    }
}

void MusicLibraryItemSong::setSong(const Song &s)
{
    m_song=s;
    if (m_genres>(void *)1) {
        delete m_genres;
        m_genres=0;
    }
}

bool MusicLibraryItemSong::hasGenre(const QString &genre) const
{
    initGenres();
    return (void *)1==m_genres ? m_song.genre==genre : allGenres().contains(genre);
}

QSet<QString> MusicLibraryItemSong::allGenres() const
{
    initGenres();
    return (void *)1==m_genres ? (QSet<QString>() << m_song.genre) : *m_genres;
}

void MusicLibraryItemSong::initGenres() const
{
    // Reduce memory usage by only storing genres in a set if required...
    if (0==m_genres) {
        QStringList g=m_song.genres();
        if (g.count()<2) {
            m_genres=(QSet<QString> *)1;
        } else {
            m_genres=new QSet<QString>();
            *m_genres=g.toSet();
        }
    }
}
