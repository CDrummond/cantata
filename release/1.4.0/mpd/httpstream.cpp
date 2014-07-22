/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#if QT_VERSION < 0x050000
#include <phonon/audiooutput.h>
#endif

HttpStream::HttpStream(QObject *p)
    : QObject(p)
    , enabled(false)
    , state(MPDState_Inactive)
    , player(0)
{
    stopOnPause=Settings::self()->stopHttpStreamOnPause();
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
            #if LIBVLC_FOUND
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
    static const char *constUrlProperty="url";
    #if LIBVLC_FOUND
    if (player) {
        libvlc_media_player_stop(player);
        libvlc_media_player_release(player);
        libvlc_release(instance);
        player=0;
    }
    #else
    if (player && player->property(constUrlProperty).toString()!=url) {
        player->stop();
        player->deleteLater();
        player=0;
    }
    #endif
    if (!url.isEmpty() && !player) {
        #if LIBVLC_FOUND
        instance = libvlc_new(0, NULL);
        QByteArray byteArrayUrl = url.toUtf8();
        media = libvlc_media_new_location(instance, byteArrayUrl.constData());
        player = libvlc_media_player_new_from_media(media);
        libvlc_media_release(media);
        #elif QT_VERSION < 0x050000
        player=new Phonon::MediaObject(this);
        Phonon::createPath(player, new Phonon::AudioOutput(Phonon::MusicCategory, this));
        player->setCurrentSource(QUrl(url));
        player->setProperty(constUrlProperty, url);
        #else
        player=new QMediaPlayer(this);
        player->setMedia(QUrl(url));
        player->setProperty(constUrlProperty, url);
        #endif
    }

    if (player) {
        state=status->state();
        switch (status->state()) {
        case MPDState_Playing:
            #if LIBVLC_FOUND
            if(libvlc_media_player_get_state(player)!=libvlc_Playing){
                libvlc_media_player_play(player);
            }
            #elif QT_VERSION < 0x050000
            if (Phonon::PlayingState!=player->state()) {
                player->play();
            }
            #else
            if (QMediaPlayer::PlayingState!=player->state()) {
                player->play();
            }
            #endif
            break;
        case MPDState_Inactive:
        case MPDState_Stopped:
            #if LIBVLC_FOUND
            libvlc_media_player_stop(player);
            #else
            player->stop();
            #endif
            break;
        case MPDState_Paused:
            if (stopOnPause) {
                #if LIBVLC_FOUND
                libvlc_media_player_stop(player);
                #else
                player->stop();
                #endif
            }
        default:
            break;
        }
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
    if (status->state()==state) {
        return;
    }

    state=status->state();
    switch (status->state()) {
    case MPDState_Playing:
        #if LIBVLC_FOUND
        if (libvlc_Playing!=libvlc_media_player_get_state(player)) {
            libvlc_media_player_play(player);
        }
        #elif QT_VERSION < 0x050000
        if (Phonon::PlayingState!=player->state()) {
            player->play();
        }
        #else
        if (QMediaPlayer::PlayingState!=player->state()) {
            player->play();
        }
        #endif
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        #if LIBVLC_FOUND
        libvlc_media_player_stop(player);
        #else
        player->stop();
        #endif
        break;
    case MPDState_Paused:
        if (stopOnPause) {
            #if LIBVLC_FOUND
            libvlc_media_player_stop(player);
            #else
            player->stop();
            #endif
        }
    default:
        break;
    }
}

