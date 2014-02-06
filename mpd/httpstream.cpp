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
#if QT_VERSION < 0x050000
#include <phonon/audiooutput.h>
#endif

HttpStream::HttpStream(QObject *p)
    : QObject(p)
    , enabled(false)
    , player(0)
{
}

void HttpStream::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    
    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(streamUrl(QString)), this, SLOT(streamUrl(QString)));
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    } else {
        disconnect(MPDConnection::self(), SIGNAL(streamUrl(QString)), this, SLOT(streamUrl(QString)));
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
        if (player) {
            player->stop();
        }
    }
}

void HttpStream::streamUrl(const QString &url)
{
    MPDStatus * const status = MPDStatus::self();
    static const char *constUrlProperty="url";
    if (player && player->property(constUrlProperty).toString()!=url) {
        player->stop();
        player->deleteLater();
        player=0;
    }
    if (player) {
        switch (status->state()) {
        case MPDState_Playing:
            player->play();
            break;
        case MPDState_Inactive:
        case MPDState_Stopped:
            player->stop();
        break;
        case MPDState_Paused:
            player->pause();
        default:
        break;
        }
    } else if (!url.isEmpty()) {
        #if QT_VERSION < 0x050000
        player=new Phonon::MediaObject(this);
        Phonon::createPath(player, new Phonon::AudioOutput(Phonon::MusicCategory, this));
        player->setCurrentSource(url);
        #else
        player=new QMediaPlayer(this);
        player->setMedia(QUrl(url));
        #endif
        player->setProperty(constUrlProperty, url);
    }
}

void HttpStream::updateStatus()
{
    if (!player) {
        return;
    }
    switch (MPDStatus::self()->state()) {
    case MPDState_Playing:
        player->play();
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        player->stop();
        break;
    case MPDState_Paused:
        player->pause();
    default:
        break;
    }
}
