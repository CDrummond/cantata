/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef HTTP_STREAM_H
#define HTTP_STREAM_H

#include <QObject>

#ifdef LIBVLC_FOUND
#include <vlc/vlc.h>
#else
#include <QtMultimedia/QMediaPlayer>
#endif

class QTimer;

class HttpStream : public QObject
{
    Q_OBJECT

public:
    static void enableDebug();
    static HttpStream * self();

    HttpStream(QObject *p=nullptr);
    ~HttpStream() override { save(); }
    void save() const;
    bool isMuted() const { return muted; }
    int volume();
    int unmuteVolume() const { return unmuteVol; }

Q_SIGNALS:
    void isEnabled(bool en);
    void update();

public Q_SLOTS:
    void setEnabled(bool e);
    void setVolume(int vol);
    void toggleMute();

private Q_SLOTS:
    void updateStatus();
    void streamUrl(const QString &url);
    void checkPlayer();
#ifndef LIBVLC_FOUND
    void bufferingProgress(int progress);
#endif

private:
    void startTimer();
    void stopTimer();

private:
    bool enabled;
    bool muted;
    int state;
    int playStateChecks;
    int currentVolume;
    int unmuteVol;
    QTimer *playStateCheckTimer;

    #ifdef LIBVLC_FOUND
    libvlc_instance_t *instance;
    libvlc_media_player_t *player;
    libvlc_media_t *media;
    #else
    QMediaPlayer *player;
    #endif
};

#endif
