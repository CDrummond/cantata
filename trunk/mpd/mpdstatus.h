/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MPD_STATUS_H
#define MPD_STATUS_H

#include <QtCore>
#include <QtGlobal>

class MPDStatus
{
public:
    enum State {
        State_Inactive,
        State_Playing,
        State_Stopped,
        State_Paused
    };

    static MPDStatus * self();

    void acquireWriteLock();
    void releaseWriteLock();

    // Getters
    quint8 volume();
    bool consume();
    bool repeat();
    bool random();
    quint32 playlist();
    qint32 playlistLength();
    qint32 playlistQueue();
    qint32 crossFade();
    State state();
    qint32 song();
    qint32 songId();
    qint32 timeElapsed();
    qint32 timeTotal();
    quint16 bitrate();
    quint16 samplerate();
    quint8 bits();
    quint8 channels();
    qint32 updatingDb();
    QString error();

    // Setters
    void setVolume(quint8 volume);
    void setConsume(bool consume);
    void setRepeat(bool repeat);
    void setRandom(bool random);
    void setPlaylist(quint32 playlist);
    void setPlaylistLength(qint32 playlist_length);
    void setPlaylistQueue(qint32 playlist_queue);
    void setCrossFade(qint32 xfade);
    void setState(State state);
    void setSong(qint32 song);
    void setSongId(qint32 song_id);
    void setTimeElapsed(qint32 time_elapsed);
    void setTimeTotal(qint32 time_total);
    void setBitrate(quint16 bitrate);
    void setSamplerate(quint16 samplerate);
    void setBits(quint8 bits);
    void setChannels(quint8 channels);
    void setUpdatingDb(qint32 updating_db);
    void setError(QString error);

private:
    quint8 m_volume;
    bool m_consume;
    bool m_repeat;
    bool m_random;
    quint32 m_playlist;
    qint32 m_playlist_length;
    qint32 m_playlist_queue;
    qint32 m_xfade;
    State m_state;
    qint32 m_song;
    qint32 m_song_id;
    qint32 m_time_elapsed;
    qint32 m_time_total;
    quint16 m_bitrate;
    quint16 m_samplerate;
    quint8 m_bits;
    quint8 m_channels;
    qint32 m_updating_db;
    QString m_error;
    QReadWriteLock m_lock;

    MPDStatus();
    ~MPDStatus() {}
    MPDStatus(const MPDStatus&);
    MPDStatus& operator=(const MPDStatus& other);
};

#endif
