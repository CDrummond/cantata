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

#include "mpdstatus.h"

MPDStatus * MPDStatus::self()
{
    static MPDStatus instance;
    return &instance;
}

MPDStatus::MPDStatus()
    : m_volume(0)
    , m_consume(false)
    , m_repeat(false)
    , m_random(false)
    , m_playlist(0)
    , m_playlist_length(-1)
    , m_playlist_queue(-1)
    , m_xfade(0)
    , m_state(MPDStatus::State_Inactive)
    , m_song(-1)
    , m_song_id(-1)
    , m_time_elapsed(-1)
    , m_time_total(-1)
    , m_bitrate(0)
    , m_samplerate(0)
    , m_bits(0)
    , m_channels(0)
    , m_updating_db(0)
{
}

void MPDStatus::acquireWriteLock()
{
    m_lock.lockForWrite();
}

void MPDStatus::releaseWriteLock()
{
    m_lock.unlock();
}

// Getters
quint8 MPDStatus::volume()
{
    m_lock.lockForRead();
    quint8 volume = m_volume;
    m_lock.unlock();
    return volume;
}

bool MPDStatus::consume()
{
    m_lock.lockForRead();
    bool consume = m_consume;
    m_lock.unlock();
    return consume;
}

bool MPDStatus::repeat()
{
    m_lock.lockForRead();
    bool repeat = m_repeat;
    m_lock.unlock();
    return repeat;
}

bool MPDStatus::random()
{
    m_lock.lockForRead();
    bool random = m_random;
    m_lock.unlock();
    return random;
}

quint32 MPDStatus::playlist()
{
    m_lock.lockForRead();
    quint32 playlist = m_playlist;
    m_lock.unlock();
    return playlist;
}

qint32 MPDStatus::playlistLength()
{
    m_lock.lockForRead();
    qint32 playlist_length = m_playlist_length;
    m_lock.unlock();
    return playlist_length;
}

qint32 MPDStatus::playlistQueue()
{
    m_lock.lockForRead();
    qint32 playlist_queue = m_playlist_queue;
    m_lock.unlock();
    return playlist_queue;
}

qint32 MPDStatus::xfade()
{
    m_lock.lockForRead();
    qint32 xfade = m_xfade;
    m_lock.unlock();
    return xfade;
}

MPDStatus::State MPDStatus::state()
{
    m_lock.lockForRead();
    State state = m_state;
    m_lock.unlock();
    return state;
}

qint32 MPDStatus::song()
{
    m_lock.lockForRead();
    qint32 song = m_song;
    m_lock.unlock();
    return song;
}

qint32 MPDStatus::songId()
{
    m_lock.lockForRead();
    qint32 song_id = m_song_id;
    m_lock.unlock();
    return song_id;
}

qint32 MPDStatus::timeElapsed()
{
    m_lock.lockForRead();
    qint32 time_elapsed = m_time_elapsed;
    m_lock.unlock();
    return time_elapsed;
}

qint32 MPDStatus::timeTotal()
{
    m_lock.lockForRead();
    qint32 time_total = m_time_total;
    m_lock.unlock();
    return time_total;
}

quint16 MPDStatus::bitrate()
{
    m_lock.lockForRead();
    quint16 bitrate = m_bitrate;
    m_lock.unlock();
    return bitrate;
}

quint16 MPDStatus::samplerate()
{
    m_lock.lockForRead();
    quint16 samplerate = m_samplerate;
    m_lock.unlock();
    return samplerate;
}

quint8 MPDStatus::bits()
{
    m_lock.lockForRead();
    quint8 bits = m_bits;
    m_lock.unlock();
    return bits;
}

quint8 MPDStatus::channels()
{
    m_lock.lockForRead();
    quint8 channels = m_channels;
    m_lock.unlock();
    return channels;
}

qint32 MPDStatus::updatingDb()
{
    m_lock.lockForRead();
    qint32 updating_db = m_updating_db;
    m_lock.unlock();
    return updating_db;
}

QString MPDStatus::error()
{
    m_lock.lockForRead();
    QString error = m_error;
    m_lock.unlock();
    return error;
}

// Setters
void MPDStatus::setVolume(quint8 volume)
{
    m_volume = volume;
}

void MPDStatus::setConsume(bool consume)
{
    m_consume = consume;
}

void MPDStatus::setRepeat(bool repeat)
{
    m_repeat = repeat;
}

void MPDStatus::setRandom(bool random)
{
    m_random = random;
}

void MPDStatus::setPlaylist(quint32 playlist)
{
    m_playlist = playlist;
}

void MPDStatus::setPlaylistLength(qint32 playlist_length)
{
    m_playlist_length = playlist_length;
}

void MPDStatus::setPlaylistQueue(qint32 playlist_queue)
{
    m_playlist_queue = playlist_queue;
}

void MPDStatus::setXfade(qint32 xfade)
{
    m_xfade = xfade;
}

void MPDStatus::setState(State state)
{
    m_state = state;
}

void MPDStatus::setSong(qint32 song)
{
    m_song = song;
}

void MPDStatus::setSongId(qint32 song_id)
{
    m_song_id = song_id;
}

void MPDStatus::setTimeElapsed(qint32 time_elapsed)
{
    m_time_elapsed = time_elapsed;
}

void MPDStatus::setTimeTotal(qint32 time_total)
{
    m_time_total = time_total;
}

void MPDStatus::setBitrate(quint16 bitrate)
{
    m_bitrate = bitrate;
}

void MPDStatus::setSamplerate(quint16 samplerate)
{
    m_samplerate = samplerate;
}

void MPDStatus::setBits(quint8 bits)
{
    m_bits = bits;
}

void MPDStatus::setChannels(quint8 channels)
{
    m_channels = channels;
}

void MPDStatus::setUpdatingDb(qint32 updating_db)
{
    m_updating_db = updating_db;
}

void MPDStatus::setError(QString error)
{
    m_error = error;
}

