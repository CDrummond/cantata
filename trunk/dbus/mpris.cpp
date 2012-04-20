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

#include "mpris.h"
#include "mpdconnection.h"
#include "playeradaptor.h"
#include "rootadaptor.h"
#include "mainwindow.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KWindowSystem>
#endif

Mpris::Mpris(MainWindow *p)
    : QObject(p)
    , mw(p)
{
    // Comes from playeradaptor.h which is auto-generated
    // in the top-level CMakeLists.txt with qt4_add_dbus_adaptor.
    new PlayerAdaptor(this);

    // Comes from rootadaptor.h which is auto-generated
    // in the top-level CMakeLists.txt with qt4_add_dbus_adaptor.
    new MediaPlayer2Adaptor(this);

    QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.cantata");
    QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this);
    connect(this, SIGNAL(goToNext()), MPDConnection::self(), SLOT(goToNext()));
    connect(this, SIGNAL(setPause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(startPlayingSong(quint32)), MPDConnection::self(), SLOT(startPlayingSong(quint32)));
    connect(this, SIGNAL(goToPrevious()), MPDConnection::self(), SLOT(goToPrevious()));
    connect(this, SIGNAL(setRandom(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(this, SIGNAL(setRepeat(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(this, SIGNAL(setSeek(quint32, quint32)), MPDConnection::self(), SLOT(setSeek(quint32, quint32)));
    connect(this, SIGNAL(setSeekId(quint32, quint32)), MPDConnection::self(), SLOT(setSeekId(quint32, quint32)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(this, SIGNAL(stopPlaying()), MPDConnection::self(), SLOT(stopPlaying()));

    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
}

void Mpris::updateCurrentSong(const Song &song)
{
    currentSong = song;
}

void Mpris::Raise()
{
    #ifdef ENABLE_KDE_SUPPORT
    mw->show();
    KWindowSystem::forceActiveWindow(mw->winId());
    #endif
}
