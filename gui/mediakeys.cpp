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

#include "mediakeys.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/gnomemediakeys.h"
#endif
#ifdef CANTATA_USE_QXT_MEDIAKEYS
#include "qxtmediakeys.h"
#endif
#include "multimediakeysinterface.h"
#include "stdactions.h"
#include "support/globalstatic.h"
#include <QDebug>
static bool debugIsEnabled=false;
#define DBUG if (debugIsEnabled) qWarning() << "MediaKeys" << __FUNCTION__
void MediaKeys::enableDebug()
{
    debugIsEnabled=true;
}

GLOBAL_STATIC(MediaKeys, instance)

MediaKeys::MediaKeys()
{
    #ifdef QT_QTDBUS_FOUND
    gnome=nullptr;
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    qxt=0;
    #endif
}

MediaKeys::~MediaKeys()
{
    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (qxt) {
        delete qxt;
    }
    #endif
    #ifdef QT_QTDBUS_FOUND
    if (gnome) {
        delete gnome;
    }
    #endif
}

void MediaKeys::start()
{
    #ifdef QT_QTDBUS_FOUND
    gnome=new GnomeMediaKeys(nullptr);
    if (activate(gnome)) {
        DBUG << "Using Gnome";
        return;
    }
    DBUG << "Gnome failed";
    gnome->deleteLater();
    gnome=nullptr;
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    qxt=new QxtMediaKeys(0);
    if (activate(qxt)) {
        DBUG << "Using Qxt";
        return;
    }
    DBUG << "Qxt failed";
    qxt->deleteLater();
    qxt=0;
    #endif
    DBUG << "None";
}

void MediaKeys::stop()
{
    #ifdef QT_QTDBUS_FOUND
    if (gnome) {
        deactivate(gnome);
        gnome->deleteLater();
        gnome=nullptr;
    }
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (qxt) {
        deactivate(qxt);
        qxt->deleteLater();
        qxt=0;
    }
    #endif
}

bool MediaKeys::activate(MultiMediaKeysInterface *iface)
{
    if (!iface) {
        return false;
    }
    if (iface->activate()) {
        QObject::connect(iface, SIGNAL(playPause()), StdActions::self()->playPauseTrackAction, SIGNAL(triggered()));
        QObject::connect(iface, SIGNAL(stop()), StdActions::self()->stopPlaybackAction, SIGNAL(triggered()));
        QObject::connect(iface, SIGNAL(next()), StdActions::self()->nextTrackAction, SIGNAL(triggered()));
        QObject::connect(iface, SIGNAL(previous()), StdActions::self()->prevTrackAction, SIGNAL(triggered()));
        return true;
    }
    return false;
}

void MediaKeys::deactivate(MultiMediaKeysInterface *iface)
{
    if (!iface) {
        return;
    }
    QObject::disconnect(iface, SIGNAL(playPause()), StdActions::self()->playPauseTrackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(stop()), StdActions::self()->stopPlaybackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(next()), StdActions::self()->nextTrackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(previous()), StdActions::self()->prevTrackAction, SIGNAL(triggered()));
    iface->deactivate();
}
