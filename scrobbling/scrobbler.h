/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * This file contains code from QMPDClient:
 * Copyright (C) 2005-2008 Håvard Tautra Knutsen <havtknut@tihlde.org>
 * Copyright (C) 2009 Voker57 <voker57@gmail.com>
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

#ifndef SCROBBLER_H
#define SCROBBLER_H

#include <QString>
#include <QStringList>
#include <QQueue>
#include <QObject>
#include <QUrl>
#include <QMap>
#include <time.h>
#include "mpd-interface/mpdstatus.h"

class QTimer;
class QNetworkReply;
class PausableTimer;
struct Song;
struct MPDStatusValues;

class Scrobbler : public QObject
{
	Q_OBJECT
public:
    struct Track {
        Track() : track(0), length(0), timestamp(0) { }
        Track(const Song &s);
        bool operator==(const Track &o) const { return track==o.track && title==o.title && artist==o.artist &&
                                                albumartist==o.albumartist && album==o.album; }
        bool operator!=(const Track &o) const { return !(*this==o); }
        void clear() { title=artist=albumartist=album=QString(); track=length=0; timestamp=0; }
        bool isEmpty() { return 0==track && 0==length && 0==timestamp && title.isEmpty() && artist.isEmpty() && albumartist.isEmpty() && album.isEmpty(); }
        QString title;
        QString artist;
        QString albumartist;
        QString album;
        quint32 track;
        quint32 length;
        time_t timestamp;
    };

    static Scrobbler * self();
    static void enableDebug();
    static const QLatin1String constCacheDir;
    static const QLatin1String constCacheFile;

    static bool viaMpd(const QString &sc) { return !sc.startsWith("http"); }

    Scrobbler();
    ~Scrobbler() override;

    QMap<QString, QString> availableScrobblers() { loadScrobblers(); return scrobblers; }
    void stop();
    bool isEnabled() const { return scrobblingEnabled; }
    bool isLoveEnabled() const { return loveIsEnabled; }
    bool lovedTrack() const { return lovePending || loveSent; }
    bool haveLoginDetails() const { return !userName.isEmpty() && !password.isEmpty(); }
    void setDetails(const QString &s, const QString &u, const QString &p);
    const QString & user() const { return userName; }
    const QString & pass() const { return password; }
    const QString & activeScrobbler() const { return scrobbler; }
    bool isAuthenticated() const { return !sessionKey.isEmpty(); }

Q_SIGNALS:
    void error(const QString &msg);
    void authenticated(bool a);
    void enabled(bool e);
    void loveEnabled(bool e);
    void songChanged(bool valid);
    void scrobblerChanged();

    // send love via client message...
    void clientMessage(const QString &client, const QString &msg, const QString &clientName);

public Q_SLOTS:
    void love();
    void setEnabled(bool e);
    void setLoveEnabled(bool e);

private Q_SLOTS:
    void setSong(const Song &s);
    void scrobbleCurrent();
    void scrobbleQueued();
    void scrobbleNowPlaying();
    void handleResp();
    void authenticate();
    void authResp();
    void scrobbleFinished();
    void mpdStateUpdated(bool songChanged=false);
    void mpdStatusUpdated(const MPDStatusValues &vals);
    void clientMessageFailed(const QString &client, const QString &msg);

private:
    void setActive();
    void loadSettings();
    bool ensureAuthenticated();
    void loadCache();
    void saveCache();
    void calcScrobbleIntervals();
    void cancelJobs();
    void reset();
    void loadScrobblers();
    QString scrobblerUrl();

private:
    bool scrobblingEnabled;
    bool loveIsEnabled;
    QMap<QString, QString> scrobblers;
    QString scrobbler;
    QString userName;
    QString password;
    QString sessionKey;
    QQueue<Track> songQueue;
    QQueue<Track> lastScrobbledSongs;
    Track inactiveSong; // Song set whilst inactive
    Track currentSong;
    PausableTimer * scrobbleTimer;
    QTimer * retryTimer;
    PausableTimer * nowPlayingTimer;
    time_t lastNowPlaying;
    QTimer * hardFailTimer;
    bool nowPlayingIsPending;
    bool lovePending;
    bool lastScrobbleFailed;
    bool nowPlayingSent;
    bool loveSent;
    bool scrobbledCurrent;
    bool scrobbleViaMpd;
    int failedCount;
    MPDState lastState;

    QNetworkReply *authJob;
    QNetworkReply *scrobbleJob;
};

#endif
