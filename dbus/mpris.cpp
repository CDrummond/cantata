/*
 * Cantata
 *
 * Copyright (c) 2011-2022 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/mpdconnection.h"
#include "playeradaptor.h"
#include "rootadaptor.h"
#include "config.h"
#include "gui/currentcover.h"

static inline qlonglong convertTime(qlonglong t)
{
    return t*1000000;
}

Mpris::Mpris(QObject *p)
    : QObject(p)
{
    QDBusConnection::sessionBus().registerService("org.mpris.MediaPlayer2.cantata");

    new PlayerAdaptor(this);
    new MediaPlayer2Adaptor(this);

    QDBusConnection::sessionBus().registerObject("/org/mpris/MediaPlayer2", this, QDBusConnection::ExportAdaptors);
    connect(this, SIGNAL(setRandom(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(this, SIGNAL(setRepeat(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(this, SIGNAL(setSingle(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(this, SIGNAL(seek(qint32)), MPDConnection::self(), SLOT(seek(qint32)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));

//    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
//    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(CurrentCover::self(), SIGNAL(coverFile(const QString &)), this, SLOT(updateCurrentCover(const QString &)));
}

Mpris::~Mpris()
{
    QDBusConnection::sessionBus().unregisterService("org.mpris.MediaPlayer2.cantata");
}

void Mpris::Pause()
{
    if (!status.isNull() && MPDState_Playing==status->state()) {
        StdActions::self()->playPauseTrackAction->trigger();
    }
}

void Mpris::Play()
{
    if (!status.isNull() && status->playlistLength()>0 && MPDState_Playing!=status->state()) {
        StdActions::self()->playPauseTrackAction->trigger();
    }
}

void Mpris::SetPosition(const QDBusObjectPath &trackId, qlonglong pos)
{
    if (trackId.path()==currentTrackId()) {
        emit setSeekId(-1, pos/1000000);
    }
}

QString Mpris::PlaybackStatus() const
{
    if (status.isNull()) {
        return QLatin1String("Stopped");
    }

    switch (status->state()) {
    case MPDState_Playing: return QLatin1String("Playing");
    case MPDState_Paused: return QLatin1String("Paused");
    default:
    case MPDState_Stopped: return QLatin1String("Stopped");
    }
}

void Mpris::SetLoopStatus(const QString &s)
{
    if (status.isNull()) {
        return;
    }

    bool repeat=(QLatin1String("None")!=s);
    bool single=(QLatin1String("Track")==s);

    if (status->repeat()!=repeat) {
        emit setRepeat(repeat);
    }
    if (status->single()!=single) {
        emit setSingle(single);
    }
}

qlonglong Mpris::Position() const
{
    // Cant use MPDStatus, as we dont poll for track position, but use a timer instead!
    //return MPDStatus::self()->timeElapsed();
    return status.isNull() ? 0 : convertTime(status->guessedElapsed());
}

void Mpris::updateStatus()
{
    updateStatus(MPDStatus::self());
}

void Mpris::updateStatus(MPDStatus * const status)
{
    QVariantMap map;

    // If the current song has not yet been updated, reject this status
    // update and wait for the next unless the play queue has recently
    // been emptied or the connection to MPD has been lost ...
    if (status->songId()!=currentSong.id && status->songId()!=-1) {
        return;
    }

    if (status!=this->status) {
        this->status = status;
    }
    if (status->repeat()!=lastStatus.repeat || status->single()!=lastStatus.single) {
        map.insert("LoopStatus", LoopStatus());
    }
    if (status->random()!=lastStatus.random) {
        map.insert("Shuffle", Shuffle());
    }
    if (status->volume()!=lastStatus.volume) {
        map.insert("Volume", Volume());
    }
    if (status->state()!=lastStatus.state || status->songId()!=lastStatus.songId || status->nextSongId()!=lastStatus.nextSongId || status->playlistLength()!=lastStatus.playlistLength) {
        map.insert("CanGoNext", CanGoNext());
        map.insert("CanGoPrevious", CanGoPrevious());
    }
    if (status->state()!=lastStatus.state) {
        map.insert("PlaybackStatus", PlaybackStatus());
        map.insert("CanPause", CanPause());
    }
    if (status->playlistLength()!=lastStatus.playlistLength) {
        map.insert("CanPlay", CanPlay());
    }
    if (status->songId()!=lastStatus.songId || status->timeTotal()!=lastStatus.timeTotal) {
        map.insert("CanSeek", CanSeek());
    }
    if (status->timeElapsed()!=lastStatus.timeElapsed) {
        map.insert("Position", convertTime(status->timeElapsed()));
    }
    if (!map.isEmpty() || status->songId()!=lastStatus.songId) {
        if (!map.contains("Position")) {
            map.insert("Position", convertTime(status->timeElapsed()));
        }
        map.insert("Metadata", Metadata());
        signalUpdate(map);
    }

    lastStatus = status->getValues();
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
    qint32 lastSongId = currentSong.id;
    currentSong = song;

    if (song.id!=lastSongId && (song.id==lastStatus.songId || -1==lastStatus.songId)) {
        // The update of the current song may come a little late.
        // So reset song ID and update status once again.
        if (-1!=lastStatus.songId) {
            lastStatus.songId = lastSongId;
        }
        updateStatus();
    } else {
        signalUpdate("Metadata", Metadata());
    }
}

QVariantMap Mpris::Metadata() const {
    QVariantMap metadataMap;
    if ((!currentSong.title.isEmpty() && !currentSong.artist.isEmpty()) || (currentSong.isStandardStream() && !currentSong.name().isEmpty())) {
        metadataMap.insert("mpris:trackid", currentTrackId());
        QString artist=currentSong.artist;
        QString album=currentSong.album;
        QString title=currentSong.title;

        if (currentSong.isStandardStream()) {
            if (artist.isEmpty()) {
                artist=currentSong.name();
            } else if (album.isEmpty()) {
                album=currentSong.name();
            }
            if (title.isEmpty()) {
                title=tr("(Stream)");
            }
        }
        if (currentSong.time>0) {
            metadataMap.insert("mpris:length", convertTime(currentSong.time));
        }
        if (!album.isEmpty()) {
            metadataMap.insert("xesam:album", album);
        }
        if (!currentSong.albumartist.isEmpty() && currentSong.albumartist!=currentSong.artist) {
            metadataMap.insert("xesam:albumArtist", QStringList() << currentSong.albumartist);
        }
        if (!artist.isEmpty()) {
            metadataMap.insert("xesam:artist", QStringList() << artist);
        }
        if (!title.isEmpty()) {
            metadataMap.insert("xesam:title", title);
        }
        if (!currentSong.genres[0].isEmpty()) {
            metadataMap.insert("xesam:genre", QStringList() << currentSong.genres[0]);
        }
        if (currentSong.track>0) {
            metadataMap.insert("xesam:trackNumber", currentSong.track);
        }
        if (currentSong.disc>0) {
            metadataMap.insert("xesam:discNumber", currentSong.disc);
        }
        if (currentSong.year>0) {
            metadataMap.insert("xesam:contentCreated", QString("%04d").arg(currentSong.year));
        }
        if (!currentSong.file.isEmpty()) {
            if (currentSong.isNonMPD()) {
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
    emit showMainWindow();
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

QString Mpris::currentTrackId() const
{
    return QString("/org/mpris/MediaPlayer2/Track/%1").arg(QString::number(currentSong.id));
}

#include "moc_mpris.cpp"
