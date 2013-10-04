/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#if !defined Q_OS_WIN && !defined Q_OS_MAC
#include "gnomemediakeys.h"
#endif
#if !defined Q_OS_MAC && QT_VERSION < 0x050000
#include "qxtmediakeys.h"
#endif
#include "stdactions.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(MediaKeys, instance)
#endif

MediaKeys * MediaKeys::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static MediaKeys *instance=0;
    if(!instance) {
        instance=new MediaKeys;
    }
    return instance;
    #endif
}

QString MediaKeys::toString(InterfaceType i)
{
    switch (i) {
    case NoInterface:
    default:
        return QString();
    case GnomeInteface:
        return "gnome";
    case QxtInterface:
        return "qxt";
    }
}

MediaKeys::InterfaceType MediaKeys::toIface(const QString &i)
{
    #if defined Q_OS_MAC
    return NoInterface;
    #endif

    if (i==toString(GnomeInteface)) {
        #if !defined Q_OS_WIN && !defined Q_OS_MAC
        return GnomeInteface;
        #else
        return NoInterface;
        #endif
    }

    if (i==toString(QxtInterface)) {
        #if QT_VERSION < 0x050000
        return QxtInterface;
        #else
        return NoInterface;
        #endif
    }
    return NoInterface;
}

MediaKeys::MediaKeys()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    gnome=0;
    #endif

    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    qxt=0;
    #endif
}

MediaKeys::~MediaKeys()
{
    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    if (qxt) {
        delete qxt;
    }
    #endif
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (gnome) {
        delete gnome;
    }
    #endif
}

void MediaKeys::load()
{
    InterfaceType current=NoInterface;
    InterfaceType configured=toIface(Settings::self()->mediaKeysIface());
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (gnome && gnome->isEnabled()) {
        current=GnomeInteface;
    }
    #endif

    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    if (qxt && qxt->isEnabled()) {
        current=QxtInterface;
    }
    #endif

    if (current==configured) {
        return;
    }

    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (gnome && GnomeInteface==current) {
        disable(gnome);
        gnome->deleteLater();
        gnome=0;
    }
    #endif

    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    if (qxt && QxtInterface==current) {
        disable(qxt);
        qxt->deleteLater();
        qxt=0;
    }
    #endif

    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (GnomeInteface==configured) {
        if (!gnome) {
            gnome=new GnomeMediaKeys(0);
        }
        enable(gnome);
    }
    #endif

    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    if (QxtInterface==configured) {
        if (!qxt) {
            qxt=new QxtMediaKeys(0);
        }
        enable(qxt);
    }
    #endif
}

void MediaKeys::enable(MultiMediaKeysInterface *iface)
{
    if (!iface || iface->isEnabled()) {
        return;
    }
    QObject::connect(iface, SIGNAL(playPause()), StdActions::self()->playPauseTrackAction, SIGNAL(triggered()));
    QObject::connect(iface, SIGNAL(stop()), StdActions::self()->stopPlaybackAction, SIGNAL(triggered()));
    QObject::connect(iface, SIGNAL(next()), StdActions::self()->nextTrackAction, SIGNAL(triggered()));
    QObject::connect(iface, SIGNAL(previous()), StdActions::self()->prevTrackAction, SIGNAL(triggered()));
    iface->setEnabled(true);
}

void MediaKeys::disable(MultiMediaKeysInterface *iface)
{
    if (!iface || !iface->isEnabled()) {
        return;
    }
    QObject::disconnect(iface, SIGNAL(playPause()), StdActions::self()->playPauseTrackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(stop()), StdActions::self()->stopPlaybackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(next()), StdActions::self()->nextTrackAction, SIGNAL(triggered()));
    QObject::disconnect(iface, SIGNAL(previous()), StdActions::self()->prevTrackAction, SIGNAL(triggered()));
    iface->setEnabled(false);
}
