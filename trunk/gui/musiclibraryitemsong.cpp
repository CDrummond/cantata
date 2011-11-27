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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"


MusicLibraryItemSong::MusicLibraryItemSong(const QString &data, MusicLibraryItem *parent)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Song),
      m_track(0),
      m_disc(0),
      m_parentItem(static_cast<MusicLibraryItemAlbum *>(parent))
{
}

MusicLibraryItemSong::~MusicLibraryItemSong()
{
}

MusicLibraryItem * MusicLibraryItemSong::parent() const
{
    return m_parentItem;
}

int MusicLibraryItemSong::row() const
{
    return m_parentItem->m_childItems.indexOf(const_cast<MusicLibraryItemSong*>(this));
}

const QString & MusicLibraryItemSong::file() const
{
    return m_file;
}

void MusicLibraryItemSong::setFile(const QString &filename)
{
    m_file = filename;
}

void MusicLibraryItemSong::setTrack(quint32 track_nr)
{
    m_track = track_nr;
}

void MusicLibraryItemSong::setDisc(quint32 disc_nr)
{
    m_disc = disc_nr;
}

quint32 MusicLibraryItemSong::track() const
{
    return m_track;
}

quint32 MusicLibraryItemSong::disc() const
{
    return m_disc;
}
