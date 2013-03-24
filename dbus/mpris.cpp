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

#include "mpris.h"
#include "mpdconnection.h"
#include "playeradaptor.h"
#include "rootadaptor.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KWindowSystem>
#endif

static inline int convertTime(int t)
{
    return t*1000000;
}

Mpris::Mpris(MainWindow *p)
    : QObject(p)
    , mw(p)
    , pos(-1)
{
    QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.cantata");

    // Comes from playeradaptor.h which is auto-generated
    // in the top-level CMakeLists.txt with qt4_add_dbus_adaptor.
    new PlayerAdaptor(this);

    // Comes from rootadaptor.h which is auto-generated
    // in the top-level CMakeLists.txt with qt4_add_dbus_adaptor.
    new MediaPlayer2Adaptor(this);

    QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this, QDBusConnection::ExportAdaptors);
    connect(this, SIGNAL(setRandom(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(this, SIGNAL(setRepeat(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(this, SIGNAL(setSeek(quint32, quint32)), MPDConnection::self(), SLOT(setSeek(quint32, quint32)));
    connect(this, SIGNAL(setSeekId(quint32, quint32)), MPDConnection::self(), SLOT(setSeekId(quint32, quint32)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));

//    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
}

qlonglong Mpris::Position() const
{
    // Cant use MPDStatus, as we dont poll for track position, but use a timer instead!
    //return MPDStatus::self()->timeElapsed();
    return convertTime(mw->currentTrackPosition());
}

void Mpris::updateStatus()
{
    QVariantMap map;

    if (MPDStatus::self()->repeat()!=status.repeat) {
        map.insert("LoopStatus", LoopStatus());
    }
    if (MPDStatus::self()->random()!=status.random) {
        map.insert("Shuffle", Shuffle());
    }
    if (MPDStatus::self()->volume()!=status.volume) {
        map.insert("Volume", Volume());
    }
    if (MPDStatus::self()->playlistLength()!=status.playlistLength) {
        map.insert("CanGoNext", CanGoNext());
        map.insert("CanGoPrevious", CanGoPrevious());
    }
    if (MPDStatus::self()->state()!=status.state) {
        map.insert("PlaybackStatus", PlaybackStatus());
        map.insert("CanPlay", CanPlay());
        map.insert("CanPause", CanPause());
    }
    if (MPDStatus::self()->timeElapsed()!=status.timeElapsed) {
        map.insert("Position", convertTime(MPDStatus::self()->timeElapsed()));
    }
    if (!map.isEmpty() || MPDStatus::self()->songId()!=status.songId) {
        if (!map.contains("Position")) {
            map.insert("Position", convertTime(MPDStatus::self()->timeElapsed()));
        }
        map.insert("Metadata", Metadata());
        signalUpdate(map);
    }
    status=MPDStatus::self()->getValues();
}

void Mpris::updateCurrentCover(const QString &fileName)
{
    if (fileName!=currentCover) {
        currentCover=fileName;
        signalUpdate("Metadata", Metadata());
    }
}

void Mpris::updateCurrentSong(const Song &song)
{
    if (song.albumArtist()!=currentSong.albumArtist() || song.album!=currentSong.album ||
        song.track!=currentSong.track || song.title!=currentSong.title || song.disc!=currentSong.disc) {
        currentSong = song;
        signalUpdate("Metadata", Metadata());
    }
}

QVariantMap Mpris::Metadata() const {
    QVariantMap metadataMap;
    if (!currentSong.title.isEmpty() && !currentSong.artist.isEmpty() &&
            (!currentSong.album.isEmpty() || (currentSong.isStream() && !currentSong.name.isEmpty()))) {
        metadataMap.insert("mpris:trackid", currentSong.id);
        if (currentSong.time>0) {
            metadataMap.insert("mpris:length", convertTime(currentSong.time));
        }
        if (!currentSong.album.isEmpty()) {
            metadataMap.insert("xesam:album", currentSong.album);
        } else if (!currentSong.name.isEmpty()) {
            metadataMap.insert("xesam:album", currentSong.name);
        }
        if (!currentSong.albumartist.isEmpty() && currentSong.albumartist!=currentSong.artist) {
            metadataMap.insert("xesam:albumArtist", QStringList() << currentSong.albumartist);
        }
        metadataMap.insert("xesam:artist", QStringList() << currentSong.artist);
        metadataMap.insert("xesam:title", currentSong.title);
        if (!currentSong.genre.isEmpty()) {
            metadataMap.insert("xesam:genre", QStringList() << currentSong.genre);
        }
        if (currentSong.track>0) {
            metadataMap.insert("xesam:trackNumber", currentSong.track);
        }
        if (currentSong.disc>0) {
            metadataMap.insert("xesam:discNumber", currentSong.disc);
        }

        if (!currentSong.file.isEmpty()) {
            if (currentSong.isStream()) {
                metadataMap.insert("xesam:url", currentSong.file);
            } else if (MPDConnection::self()->getDetails().dirReadable) {
                QString mpdDir=MPDConnection::self()->getDetails().dir;
                if (!mpdDir.isEmpty()) {
                    metadataMap.insert("xesam:url", "file://"+mpdDir+currentSong.file);
                }
            }
        }
        if (!currentCover.isEmpty()) {
             metadataMap.insert("mpris:artUrl", "file://"+currentCover);
        }
    }

    return metadataMap;
}

void Mpris::Raise()
{
    #ifdef ENABLE_KDE_SUPPORT
    mw->show();
    KWindowSystem::forceActiveWindow(mw->effectiveWinId());
    #else
    mw->restoreWindow();
    #endif
}

void Mpris::signalUpdate(const QString &property, const QVariant &value)
{
    QVariantMap map;
    map.insert(property, value);
    signalUpdate(map);
}

void Mpris::signalUpdate(const QVariantMap &map)
{
    if (map.isEmpty()) {
        return;
    }
    QDBusMessage signal = QDBusMessage::createSignal("/org/mpris/MediaPlayer2",
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged");
    QVariantList args = QVariantList()
                          << "org.mpris.MediaPlayer2.Player"
                          << map
                          << QStringList();
    signal.setArguments(args);
    QDBusConnection::sessionBus().send(signal);
}
