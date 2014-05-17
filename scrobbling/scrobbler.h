/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QQueue>
#include <QObject>
#include <QUrl>
#include <time.h>

class QTimer;
class QNetworkReply;
class PausableTimer;
class Song;

class Scrobbler : public QObject
{
	Q_OBJECT
public:
    struct Track {
        Track() : track(0), length(0), timestamp(0) { }
        Track(const Song &s);
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

    Scrobbler();
    ~Scrobbler();

    void stop();
    bool isEnabled() const { return scrobblingEnabled && haveLoginDetails(); }
    bool haveLoginDetails() const { return !userName.isEmpty() && !password.isEmpty(); }
    void setDetails(const QString &u, const QString &p);
    const QString & user() const { return userName; }
    const QString & pass() const { return password; }
    bool isAuthenticated() const { return !sessionKey.isEmpty(); }
    bool isScrobblingEnabled() const { return scrobblingEnabled; }

Q_SIGNALS:
    void error(const QString &msg);
    void authenticated(bool a);
    void enabled(bool e);

public Q_SLOTS:
    void setEnabled(bool e);

private Q_SLOTS:
    void setSong(const Song &s);
    void scrobbleCurrent();
    void scrobbleQueued();
    void sendNowPlaying();
    void nowPlayingResp();
    void authenticate();
    void authResp();
    void scrobbleFinished();
    void mpdStateUpdated();

private:
    void setActive();
    void loadSettings();
    void handle(const QString &status);
    bool ensureAuthenticated();
    void scrobbleNowPlaying(const Track &s);
    void loadCache();
    void saveCache();
    void cancelJobs();

private:
    bool scrobblingEnabled;
    QString scrobbler;
    QString userName;
    QString password;
    QString sessionKey;
    QString nowPlayingUrl;
    QString scrobbleUrl;
    QQueue<Track> songQueue;
    QQueue<Track> lastScrobbledSongs;
    Track currentSong;
    PausableTimer * scrobbleTimer;
    QTimer * retryTimer;
    PausableTimer * nowPlayingTimer;
    QTimer * hardFailTimer;
    bool nowPlayingIsPending;
    bool lastScrobbleFailed;
    int failedCount;

    QNetworkReply *authJob;
    QNetworkReply *scrobbleJob;
};

#endif
