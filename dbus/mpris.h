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

#ifndef MPRIS_H
#define MPRIS_H

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QApplication>

#include "song.h"
#include "mpdstatus.h"
#include "mainwindow.h"

class QDBusObjectPath;

class Mpris : public QObject
{
    Q_OBJECT

    // org.mpris.MediaPlayer2.Player
    Q_PROPERTY( double Rate READ Rate WRITE SetRate )
    Q_PROPERTY( qlonglong Position READ Position )
    Q_PROPERTY( double MinimumRate READ MinimumRate )
    Q_PROPERTY( double MaximumRate READ MaximumRate )
    Q_PROPERTY( bool CanControl READ CanControl )
    Q_PROPERTY( bool CanPlay READ CanPlay )
    Q_PROPERTY( bool CanPause READ CanPause )
    Q_PROPERTY( bool CanGoNext READ CanGoNext )
    Q_PROPERTY( bool CanGoPrevious READ CanGoPrevious )
    Q_PROPERTY( QString PlaybackStatus READ PlaybackStatus )
    Q_PROPERTY( QString LoopStatus READ LoopStatus WRITE SetLoopStatus )
    Q_PROPERTY( bool Shuffle READ Shuffle WRITE SetShuffle )
    Q_PROPERTY( QVariantMap Metadata READ Metadata )
    Q_PROPERTY( double Volume READ Volume WRITE SetVolume )

    // org.mpris.MediaPlayer2
    Q_PROPERTY( bool CanQuit READ CanQuit )
    Q_PROPERTY( bool CanRaise READ CanRaise )
    Q_PROPERTY( QString DesktopEntry READ DesktopEntry )
    Q_PROPERTY( bool HasTrackList READ HasTrackList )
    Q_PROPERTY( QString Identity READ Identity )
    Q_PROPERTY( QStringList SupportedMimeTypes READ SupportedMimeTypes )
    Q_PROPERTY( QStringList SupportedUriSchemes READ SupportedUriSchemes )

public:
    Mpris(MainWindow *p);

    virtual ~Mpris() { }

    // org.mpris.MediaPlayer2.Player
    void Next() { mw->nextTrack(); }
    void Previous() { mw->prevTrack(); }
    void Pause() {
        if (MPDState_Playing==MPDStatus::self()->state()) {
            mw->playPauseTrack();
        }
    }

    void PlayPause() {  mw->playPauseTrack(); }
    void Stop() {  mw->stopTrack(); }

    void Play() {
        MPDStatus * const status = MPDStatus::self();

        if (status->playlistLength() && MPDState_Playing!=status->state()) {
           mw->playPauseTrack();
        }
    }

    void Seek(qlonglong) { }
    void SetPosition(const QDBusObjectPath &, qlonglong) { }
    void OpenUri(const QString &) { }

    QString PlaybackStatus() {
        switch(MPDStatus::self()->state()) {
        case MPDState_Playing: return QLatin1String("Playing");
        case MPDState_Paused: return QLatin1String("Paused");
        default:
        case MPDState_Stopped: return QLatin1String("Stopped");
        }
    }

    QString LoopStatus() { return MPDStatus::self()->repeat() ? QLatin1String("Playlist") : QLatin1String("None"); }
    void SetLoopStatus(const QString &s) { emit setRepeat(QLatin1String("None")!=s); }
    QVariantMap Metadata() const;
    int Rate() const { return 1.0; }
    void SetRate(double) { }
    bool Shuffle() { return MPDStatus::self()->random(); }
    void SetShuffle(bool s) { emit setRandom(s); }
    double Volume() const { return MPDStatus::self()->volume()/100.0; }
    void SetVolume(double v) { emit setVolume(v*100); }
    qlonglong Position() const;
    double MinimumRate() const { return 1.0; }
    double MaximumRate() const { return 1.0; }
    bool CanControl() const { return true; }
    bool CanPlay() const { return MPDState_Playing!=MPDStatus::self()->state() && MPDStatus::self()->playlistLength()>0; }
    bool CanPause() const { return MPDState_Playing==MPDStatus::self()->state(); }
    bool CanGoNext() const { return MPDState_Stopped!=MPDStatus::self()->state() && MPDStatus::self()->playlistLength()>1; }
    bool CanGoPrevious() const { return MPDState_Stopped!=MPDStatus::self()->state() && MPDStatus::self()->playlistLength()>1; }

    // org.mpris.MediaPlayer2
    bool CanQuit() const { return true; }
    bool CanRaise() const { return true; }
    bool HasTrackList() const { return false; }
    QString Identity() const { return QLatin1String("Cantata"); }
    QString DesktopEntry() const {
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

    QStringList SupportedUriSchemes() const { return QStringList(); }
    QStringList SupportedMimeTypes() const { return QStringList(); }

public:
    void updateCurrentSong(const Song &song);

public Q_SLOTS:
    void Raise();

    void Quit() { QApplication::quit(); }

Q_SIGNALS:
    // org.mpris.MediaPlayer2.Player
    void setRandom(bool toggle);
    void setRepeat(bool toggle);
    void setSeek(quint32 song, quint32 time);
    void setSeekId(quint32 songId, quint32 time);
    void setVolume(int vol);

public Q_SLOTS:
    void updateCurrentCover(const QString &fileName);

private Q_SLOTS:
    void updateStatus();

private:
    void signalUpdate(const QString &property, const QVariant &value);
    void signalUpdate(const QVariantMap &map);

private:
    MainWindow *mw;
    MPDStatusValues status;
    QString currentCover;
    Song currentSong;
    int pos;
};

#endif
