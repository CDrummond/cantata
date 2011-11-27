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

#include "mpdstats.h"




MPDStats * MPDStats::self()
{
    static MPDStats instance;
    return &instance;
}

MPDStats::MPDStats()
    : m_artists(0)
    , m_albums(0)
    , m_songs(0)
    , m_uptime(0)
    , m_playtime(0)
    , m_db_playtime(0)
    , m_playlist_artists(0)
    , m_playlist_albums(0)
    , m_playlist_songs(0)
    , m_playlist_time(0)
{
}

void MPDStats::acquireWriteLock()
{
    m_lock.lockForWrite();
}

void MPDStats::releaseWriteLock()
{
    m_lock.unlock();
}

// Getters
quint32 MPDStats::artists()
{
    m_lock.lockForRead();
    quint32 artists = m_artists;
    m_lock.unlock();
    return artists;
}

quint32 MPDStats::albums()
{
    m_lock.lockForRead();
    quint32 albums = m_albums;
    m_lock.unlock();
    return albums;
}

quint32 MPDStats::songs()
{
    m_lock.lockForRead();
    quint32 songs = m_songs;
    m_lock.unlock();
    return songs;
}

quint32 MPDStats::uptime()
{
    m_lock.lockForRead();
    quint32 uptime = m_uptime;
    m_lock.unlock();
    return uptime;
}

quint32 MPDStats::playtime()
{
    m_lock.lockForRead();
    quint32 playtime = m_playtime;
    m_lock.unlock();
    return playtime;
}

quint32 MPDStats::dbPlaytime()
{
    m_lock.lockForRead();
    quint32 db_playtime = m_db_playtime;
    m_lock.unlock();
    return db_playtime;
}

QDateTime MPDStats::dbUpdate()
{
    m_lock.lockForRead();
    QDateTime db_update = m_db_update;
    m_lock.unlock();
    return db_update;
}

quint32 MPDStats::playlistArtists()
{
    m_lock.lockForRead();
    quint32 playlistArtists = m_playlist_artists;
    m_lock.unlock();
    return playlistArtists;
}

quint32 MPDStats::playlistAlbums()
{
    m_lock.lockForRead();
    quint32 playlistAlbums = m_playlist_albums;
    m_lock.unlock();
    return playlistAlbums;
}

quint32 MPDStats::playlistSongs()
{
    m_lock.lockForRead();
    quint32 playlistSongs = m_playlist_songs;
    m_lock.unlock();
    return playlistSongs;
}

quint32 MPDStats::playlistTime()
{
    m_lock.lockForRead();
    quint32 playlistTime = m_playlist_time;
    m_lock.unlock();
    return playlistTime;
}

// Setters
void MPDStats::setArtists(quint32 artists)
{
    m_artists = artists;
}

void MPDStats::setAlbums(quint32 albums)
{
    m_albums = albums;
}

void MPDStats::setSongs(quint32 songs)
{
    m_songs = songs;
}

void MPDStats::setUptime(quint32 uptime)
{
    m_uptime = uptime;
}

void MPDStats::setPlaytime(quint32 playtime)
{
    m_playtime = playtime;
}

void MPDStats::setDbPlaytime(quint32 db_playtime)
{
    m_db_playtime = db_playtime;
}

void MPDStats::setDbUpdate(uint seconds)
{
    m_db_update.setTime_t(seconds);
}

void MPDStats::setPlaylistArtists(quint32 artists)
{
    m_playlist_artists = artists;
}

void MPDStats::setPlaylistAlbums(quint32 albums)
{
    m_playlist_albums = albums;
}

void MPDStats::setPlaylistSongs(quint32 songs)
{
    m_playlist_songs = songs;
}

void MPDStats::setPlaylistTime(quint32 time)
{
    m_playlist_time = time;
}
