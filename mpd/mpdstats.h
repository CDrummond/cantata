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

#ifndef MPD_STATS_H
#define MPD_STATS_H

#include <QDateTime>
#include <QReadWriteLock>

class MPDStats
{
public:
    static MPDStats * self();

    void acquireWriteLock();
    void releaseWriteLock();

    // Getters
    quint32 artists();
    quint32 albums();
    quint32 songs();
    quint32 uptime();
    quint32 playtime();
    quint32 dbPlaytime();
    QDateTime dbUpdate();
    quint32 playlistArtists();
    quint32 playlistAlbums();
    quint32 playlistSongs();
    quint32 playlistTime();

    // Setters
    void setArtists(quint32 artists);
    void setAlbums(quint32 albums);
    void setSongs(quint32 songs);
    void setUptime(quint32 uptime);
    void setPlaytime(quint32 playtime);
    void setDbPlaytime(quint32 db_playtime);
    void setDbUpdate(uint seconds);
    void setPlaylistArtists(quint32 artists);
    void setPlaylistAlbums(quint32 albums);
    void setPlaylistSongs(quint32 songs);
    void setPlaylistTime(quint32 time);

private:
    quint32 m_artists;
    quint32 m_albums;
    quint32 m_songs;
    quint32 m_uptime;
    quint32 m_playtime;
    quint32 m_db_playtime;
    QDateTime m_db_update;
    QReadWriteLock m_lock;
    quint32 m_playlist_artists;
    quint32 m_playlist_albums;
    quint32 m_playlist_songs;
    quint32 m_playlist_time;

    MPDStats();
    ~MPDStats() {}
    MPDStats(const MPDStats&);
    MPDStats& operator=(const MPDStats& other);
};

#endif
