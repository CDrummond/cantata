/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "httpstream.h"
#include "mpdconnection.h"
#include "mpdstatus.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#include "support/configuration.h"
#ifndef LIBVLC_FOUND
#include <QtMultimedia/QMediaPlayer>
#endif
#include <QTimer>

static const int constPlayerCheckPeriod=250;
static const int constMaxPlayStateChecks=2000/constPlayerCheckPeriod;

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void HttpStream::enableDebug()
{
    debugEnabled=true;
}

GLOBAL_STATIC(HttpStream, instance)

HttpStream::HttpStream(QObject *p)
    : QObject(p)
    , enabled(false)
    , state(MPDState_Inactive)
    , muted(false)
    , playStateChecks(0)
    , currentVolume(50)
    , unmuteVol(50)
    , playStateCheckTimer(nullptr)
    , player(nullptr)
{
}

void HttpStream::save() const
{
    Configuration config(metaObject()->className());
    config.set("volume", currentVolume);
}

void HttpStream::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }

    enabled=e;
    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(streamUrl(QString)), this, SLOT(streamUrl(QString)));
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
        streamUrl(MPDConnection::self()->getDetails().streamUrl);
    } else {
        disconnect(MPDConnection::self(), SIGNAL(streamUrl(QString)), this, SLOT(streamUrl(QString)));
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
        if (player) {
            save();
            #ifdef LIBVLC_FOUND
            libvlc_media_player_stop(player);
            #else
            player->stop();
            #endif
        }
    }
    emit isEnabled(enabled);
}

void HttpStream::setVolume(int vol)
{
    DBUG << vol;
    if (player) {
        currentVolume=vol;
        #ifdef LIBVLC_FOUND
        libvlc_audio_set_volume(player, vol);
        #else
        player->setVolume(vol);
        #endif
        emit update();
    }
}

int HttpStream::volume()
{
    int vol=currentVolume;
    if (player && !isMuted()) {
        #ifdef LIBVLC_FOUND
        vol=libvlc_audio_get_volume(player);
        #else
        vol=player->volume();
        #endif
        if (vol<0) {
            vol=currentVolume;
        } else {
            currentVolume=vol;
        }
    }
    DBUG << vol;
    return vol;
}

void HttpStream::toggleMute()
{
    DBUG << isMuted();
    if (player) {
        muted=!muted;
        #ifdef LIBVLC_FOUND
        libvlc_audio_set_mute(player, muted);
        #else
        player->setMuted(!muted);
        #endif
        emit update();
    }
}

void HttpStream::streamUrl(const QString &url)
{
    MPDStatus * const status = MPDStatus::self();
    DBUG << url;
    #ifdef LIBVLC_FOUND
    if (player) {
        libvlc_media_player_stop(player);
        libvlc_media_player_release(player);
        libvlc_release(instance);
        player=0;
    }
    #else
    static const char *constUrlProperty="url";
    if (player && player->property(constUrlProperty).toString()!=url) {
        player->stop();
        player->deleteLater();
        player=nullptr;
    }
    #endif
    if (!url.isEmpty() && !player) {
        #ifdef LIBVLC_FOUND
        instance = libvlc_new(0, NULL);
        QByteArray byteArrayUrl = url.toUtf8();
        media = libvlc_media_new_location(instance, byteArrayUrl.constData());
        player = libvlc_media_player_new_from_media(media);
        libvlc_media_release(media);
        #else
        player=new QMediaPlayer(this);
        player->setMedia(QUrl(url));
        player->setProperty(constUrlProperty, url);
        #endif
        muted=false;
        setVolume(Configuration(metaObject()->className()).get("volume", currentVolume));
    }
    if (player) {
        state=0xFFFF; // Force an update...
        updateStatus();
    } else {
        state=MPDState_Inactive;
    }
    emit update();
}

void HttpStream::updateStatus()
{
    if (!player) {
        return;
    }

    MPDStatus * const status = MPDStatus::self();
    DBUG << status->state() << state;
    if (status->state()==state) {
        return;
    }

    state=status->state();
    switch (status->state()) {
    case MPDState_Playing:
        // Only start playback if not aready playing
        #ifdef LIBVLC_FOUND
        if (libvlc_Playing!=libvlc_media_player_get_state(player)) {
            libvlc_media_player_play(player);
            startTimer();
        }
        #else
        if (QMediaPlayer::PlayingState!=player->state()) {
            player->play();
            startTimer();
        }
        #endif
        break;
    case MPDState_Paused:
    case MPDState_Inactive:
    case MPDState_Stopped:
        #ifdef LIBVLC_FOUND
        libvlc_media_player_stop(player);
        #else
        player->stop();
        #endif
        stopTimer();
        break;
    default:
        stopTimer();
        break;
    }
}

void HttpStream::checkPlayer()
{
    if (0==--playStateChecks) {
        stopTimer();
        DBUG << "Max checks reached";
    }
    #ifdef LIBVLC_FOUND
    if (libvlc_Playing==libvlc_media_player_get_state(player)) {
        DBUG << "Playing";
        stopTimer();
    } else {
        DBUG << "Try again";
        libvlc_media_player_play(player);
    }
    #else
    if (QMediaPlayer::PlayingState==player->state()) {
        DBUG << "Playing";
        stopTimer();
    } else {
        DBUG << "Try again";
        player->play();
    }
    #endif
}

void HttpStream::startTimer()
{
    if (!playStateCheckTimer) {
        playStateCheckTimer=new QTimer(this);
        playStateCheckTimer->setSingleShot(false);
        playStateCheckTimer->setInterval(constPlayerCheckPeriod);
        connect(playStateCheckTimer, SIGNAL(timeout()), SLOT(checkPlayer()));
    }
    playStateChecks = constMaxPlayStateChecks;
    DBUG << playStateChecks;
    playStateCheckTimer->start();
}

void HttpStream::stopTimer()
{
    if (playStateCheckTimer) {
        DBUG;
        playStateCheckTimer->stop();
    }
    playStateChecks=0;
}

#include "moc_httpstream.cpp"
