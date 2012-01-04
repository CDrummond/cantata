/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MPRIS_H
#define MPRIS_H

#include <QtCore/QObject>
#include "mpdstatus.h"

class QDBusObjectPath;

class Mpris : public QObject
{
    Q_OBJECT

    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY( double Rate READ Rate WRITE SetRate )
    Q_PROPERTY( qlonglong Position READ Position )
    Q_PROPERTY( double MinimumRate READ MinimumRate )
    Q_PROPERTY( double MaximumRate READ MaximumRate )
    Q_PROPERTY( bool CanControl READ CanControl )
    Q_PROPERTY( QString PlaybackStatus READ PlaybackStatus )
    Q_PROPERTY( QString LoopStatus READ LoopStatus WRITE SetLoopStatus)

public:
    Mpris(QObject *p);

    virtual ~Mpris() {
    }

    void Next() {
        emit goToNext();
    }

    void Previous() {
        emit goToPrevious();
    }

    void Pause() {
        emit setPause(true);
    }

    void PlayPause() {
        MPDStatus * const status = MPDStatus::self();

        if (MPDStatus::State_Playing==status->state()) {
            emit setPause(true);
        } else if (MPDStatus::State_Paused==status->state()) {
            emit setPause(false);
        } else {
            emit startPlayingSong();
        }
    }

    void Stop() {
        emit stopPlaying();
    }

    void Play() {
        emit startPlayingSong();
    }

    void Seek(qlonglong) {
    }

    void SetPosition(const QDBusObjectPath &, qlonglong) {
    }

    void OpenUri(const QString &) {
    }

    QString PlaybackStatus() {
        switch(MPDStatus::self()->state()) {
        case MPDStatus::State_Playing: return QLatin1String("Playing");
        case MPDStatus::State_Paused: return QLatin1String("Paused");
        default:
        case MPDStatus::State_Stopped: return QLatin1String("Stopped");
        }
    }

    QString LoopStatus() {
        return MPDStatus::self()->repeat() ? QLatin1String("Playlist") : QLatin1String("None");
    }

    void SetLoopStatus(const QString &s) {
        emit setRepeat(QLatin1String("None")!=s);
    }

    int Rate() const {
        return 1.0;
    }

    void SetRate(double) {
    }

    void SetShuffle(bool s) {
        emit setRandom(s);
    }

    double Volume() const {
        return MPDStatus::self()->volume()/100.0;
    }

    void SetVolume(double v) {
        emit setVolume(v*100);
    }

    qlonglong Position() const {
        return MPDStatus::self()->timeElapsed();
    }

    double MinimumRate() const {
        return 1.0;
    }

    double MaximumRate() const {
        return 1.0;
    }

    bool CanControl() const {
        return true;
    }

Q_SIGNALS:
    void goToNext();
    void setPause(bool toggle);
    void startPlayingSong(quint32 song = 0);
    void goToPrevious();
    void setRandom(bool toggle);
    void setRepeat(bool toggle);
    void setSeek(quint32 song, quint32 time);
    void setSeekId(quint32 songId, quint32 time);
    void setVolume(int vol);
    void stopPlaying();
};

#endif
