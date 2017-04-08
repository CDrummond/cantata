/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtMultimedia/QMediaPlayer>
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

HttpStream::HttpStream(QObject *p)
    : QObject(p)
    , enabled(false)
    , state(MPDState_Inactive)
    , playStateChecks(0)
    , playStateCheckTimer(0)
    , player(0)
{
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
            #ifdef LIBVLC_FOUND
            libvlc_media_player_stop(player);
            #else
            player->stop();
            #endif
        }
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
        player=0;
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
    }

    if (player) {
        state=0xFFFF; // Force an update...
        updateStatus();
    } else {
        state=MPDState_Inactive;
    }
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
        #ifdef LIBVLC_FOUND
        libvlc_media_player_play(player);
        #else
        player->play();
        #endif
        startTimer();
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        #ifdef LIBVLC_FOUND
        libvlc_media_player_stop(player);
        #else
        player->stop();
        #endif
        stopTimer();
        break;
    case MPDState_Paused:
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
