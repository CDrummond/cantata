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

#ifndef MPRIS_H
#define MPRIS_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtGui/QApplication>

#include "song.h"
#include "mpdstatus.h"

class QDBusObjectPath;
class MainWindow;

class Mpris : public QObject
{
    Q_OBJECT

    // org.mpris.MediaPlayer2.Player
    Q_PROPERTY( double Rate READ Rate WRITE SetRate )
    Q_PROPERTY( qlonglong Position READ Position )
    Q_PROPERTY( double MinimumRate READ MinimumRate )
    Q_PROPERTY( double MaximumRate READ MaximumRate )
    Q_PROPERTY( bool CanControl READ CanControl )
    Q_PROPERTY( QString PlaybackStatus READ PlaybackStatus )
    Q_PROPERTY( QString LoopStatus READ LoopStatus WRITE SetLoopStatus)
    Q_PROPERTY( QVariantMap Metadata READ metadata )

    // org.mpris.MediaPlayer2
    Q_PROPERTY( bool CanQuit READ canQuit )
    Q_PROPERTY( bool CanRaise READ canRaise )
    Q_PROPERTY( QString DesktopEntry READ desktopEntry )
    Q_PROPERTY( bool HasTrackList READ hasTrackList )
    Q_PROPERTY( QString Identity READ identity )
    Q_PROPERTY( QStringList SupportedMimeTypes READ supportedMimeTypes )
    Q_PROPERTY( QStringList SupportedUriSchemes READ supportedUriSchemes )


public:
    Mpris(MainWindow *p);

    virtual ~Mpris() {
    }

    // org.mpris.MediaPlayer2.Player
    void Next() {
        if (MPDStatus::self()->playlistLength()>1) {
            emit goToNext();
        }
    }

    void Previous() {
        if (MPDStatus::self()->playlistLength()>1) {
            emit goToPrevious();
        }
    }

    void Pause() {
        if (MPDState_Playing==MPDStatus::self()->state()) {
            emit setPause(true);
        }
    }

    void PlayPause() {
        MPDStatus * const status = MPDStatus::self();

        if (status->playlistLength()) {
            if (MPDState_Playing==status->state()) {
                emit setPause(true);
            } else if (MPDState_Paused==status->state()) {
                emit setPause(false);
            } else {
                emit startPlayingSong();
            }
        }
    }

    void Stop() {
        MPDStatus * const status = MPDStatus::self();
        if (MPDState_Playing==status->state() || MPDState_Paused==status->state()) {
            emit stopPlaying();
        }
    }

    void Play() {
        MPDStatus * const status = MPDStatus::self();

        if (status->playlistLength() && MPDState_Playing!=status->state()) {
            emit startPlayingSong();
        }
    }

    void Seek(qlonglong) {
    }

    void SetPosition(const QDBusObjectPath &, qlonglong) {
    }

    void OpenUri(const QString &) {
    }

    QString PlaybackStatus() {
        switch(MPDStatus::self()->state()) {
        case MPDState_Playing: return QLatin1String("Playing");
        case MPDState_Paused: return QLatin1String("Paused");
        default:
        case MPDState_Stopped: return QLatin1String("Stopped");
        }
    }

    QString LoopStatus() {
        return MPDStatus::self()->repeat() ? QLatin1String("Playlist") : QLatin1String("None");
    }

    void SetLoopStatus(const QString &s) {
        emit setRepeat(QLatin1String("None")!=s);
    }

    QVariantMap metadata() const {
        QVariantMap metadataMap;

        if (!currentSong.title.isEmpty() && !currentSong.artist.isEmpty() && !currentSong.album.isEmpty()) {
            metadataMap.insert("mpris:trackid", currentSong.id);
            metadataMap.insert("mpris:length", currentSong.time / 1000 / 1000);
            metadataMap.insert("xesam:album", currentSong.album);
            metadataMap.insert("xesam:artist", currentSong.artist);
            metadataMap.insert("xesam:title", currentSong.title);
        }

        return metadataMap;
    }

    int Rate() const {
        return 1.0;
    }

    void SetRate(double) {
    }

    void SetShuffle(bool s) {
        emit setRandom(s);
    }

    double Volume() const {
        return MPDStatus::self()->volume()/100.0;
    }

    void SetVolume(double v) {
        emit setVolume(v*100);
    }

    qlonglong Position() const {
        return MPDStatus::self()->timeElapsed();
    }

    double MinimumRate() const {
        return 1.0;
    }

    double MaximumRate() const {
        return 1.0;
    }

    bool CanControl() const {
        return true;
    }

    // org.mpris.MediaPlayer2
    bool canQuit() const {
        return true;
    }

    bool canRaise() const {
        #ifdef ENABLE_KDE_SUPPORT
        return true;
        #else
        return false;
        #endif
    }

    bool hasTrackList() const {
        return false;
    }

    QString identity() const {
        return QLatin1String("Cantata");
    }

    QString desktopEntry() const {
        #ifdef ENABLE_KDE_SUPPORT
        // Desktop file is installed in $prefix/share/applications/kde4/
        // rather than in $prefix/share/applications. The standard way to
        // represent this dir is with a "kde4-" prefix. See:
        // http://standards.freedesktop.org/menu-spec/1.0/go01.html#term-desktop-file-id
        return QLatin1String("kde4-cantata");
        #else
        return QLatin1String("cantata");
        #endif
    }

    QStringList supportedUriSchemes() const {
        return QStringList();
    }

    QStringList supportedMimeTypes() const {
        return QStringList();
    }

public slots:
    void Raise();

    void Quit() {
        QApplication::quit();
    }

    // org.kde.cantata.Mpris2Extensions.Player
    void showPage(const QString &page, bool focusSearch);

Q_SIGNALS:
    // org.mpris.MediaPlayer2.Player
    void goToNext();
    void setPause(bool toggle);
    void startPlayingSong(quint32 song = 0);
    void goToPrevious();
    void setRandom(bool toggle);
    void setRepeat(bool toggle);
    void setSeek(quint32 song, quint32 time);
    void setSeekId(quint32 songId, quint32 time);
    void setVolume(int vol);
    void stopPlaying();

private slots:
    void updateCurrentSong(const Song &song);

private:
    MainWindow *mw;
    Song currentSong;
};

#endif
