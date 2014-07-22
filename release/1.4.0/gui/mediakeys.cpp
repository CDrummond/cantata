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

#include "mediakeys.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/gnomemediakeys.h"
#endif
#ifdef CANTATA_USE_QXT_MEDIAKEYS
#include "qxtmediakeys.h"
#endif
#include "multimediakeysinterface.h"
#include "stdactions.h"
#include "settings.h"
#include "support/globalstatic.h"

GLOBAL_STATIC(MediaKeys, instance)

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
        #ifdef QT_QTDBUS_FOUND
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
    #ifdef QT_QTDBUS_FOUND
    gnome=0;
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

void MediaKeys::load()
{
    InterfaceType current=NoInterface;
    InterfaceType configured=toIface(Settings::self()->mediaKeysIface());
    #ifdef QT_QTDBUS_FOUND
    if (gnome && gnome->isEnabled()) {
        current=GnomeInteface;
    }
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (qxt && qxt->isEnabled()) {
        current=QxtInterface;
    }
    #endif

    if (current==configured) {
        return;
    }

    #ifdef QT_QTDBUS_FOUND
    if (gnome && GnomeInteface==current) {
        disable(gnome);
        gnome->deleteLater();
        gnome=0;
    }
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (qxt && QxtInterface==current) {
        disable(qxt);
        qxt->deleteLater();
        qxt=0;
    }
    #endif

    #ifdef QT_QTDBUS_FOUND
    if (GnomeInteface==configured) {
        if (!gnome) {
            gnome=new GnomeMediaKeys(0);
        }
        enable(gnome);
    }
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (QxtInterface==configured) {
        if (!qxt) {
            qxt=new QxtMediaKeys(0);
        }
        enable(qxt);
    }
    #endif
}

void MediaKeys::stop()
{
    #ifdef QT_QTDBUS_FOUND
    if (gnome) {
        disable(gnome);
        gnome->deleteLater();
        gnome=0;
    }
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    if (qxt) {
        disable(qxt);
        qxt->deleteLater();
        qxt=0;
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

