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

#include "scrobbler.h"
#include "pausabletimer.h"
#include "config.h"
#include "gui/covers.h"
#include "network/networkaccessmanager.h"
#include "mpd/mpdstatus.h"
#include "mpd/mpdconnection.h"
#include "support/localize.h"
#include "support/globalstatic.h"
#include "support/utils.h"
#include "support/configuration.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QUrl>
#include <QStringList>
#include <QCryptographicHash>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QXmlStreamReader>

#include <QDebug>
static bool debugIsEnabled=false;
#define DBUG if (debugIsEnabled) qWarning() << metaObject()->className() << __FUNCTION__
#define DBUG_CLASS(C) if (debugIsEnabled) qWarning() << C << __FUNCTION__
void Scrobbler::enableDebug()
{
    debugIsEnabled=true;
}

const QLatin1String Scrobbler::constCacheDir("scrobbling");
const QLatin1String Scrobbler::constCacheFile("tracks.xml.gz");
static const QLatin1String constOk("OK");
static const QLatin1String constBadSession("BADSESSION");
static const QLatin1String constFailed("FAILED");
static const QLatin1String constSettingsGroup("Scrobbling");
static const QString constSecretKey=QLatin1String("0753a75ccded9b17b872744d4bb60b35");
static const QString constUrl=QLatin1String("http://ws.audioscrobbler.com/2.0/");
static const int constMaxBatchSize=50;

GLOBAL_STATIC(Scrobbler, instance)

enum LastFmErrors
{
    InvalidService = 2,
    InvalidMethod,
    AuthenticationFailed,
    InvalidFormat,
    InvalidParameters,
    InvalidResourceSpecified,
    OperationFailed,
    InvalidSessionKey,
    InvalidApiKey,
    ServiceOffline,

    TokenNotAuthorised = 14,

    TryAgainLater = 16,

    RateLimitExceeded = 29
};

static QString lastFmError(int code, const QString &msg)
{
    switch (code) {
    case InvalidService: return i18n("Invalid service");
    case InvalidMethod: return i18n("Invalid method");
    case AuthenticationFailed: return i18n("Authentication failed");
    case InvalidFormat: return i18n("Invalid format");
    case InvalidParameters: return i18n("Invalid parameters");
    case InvalidResourceSpecified: return i18n("Invalid resource specified");
    case OperationFailed: return i18n("Operation failed");
    case InvalidSessionKey: return i18n("Invalid session key");
    case InvalidApiKey: return i18n("Invalid API key");
    case ServiceOffline: return i18n("Service offline");
    case TryAgainLater: return i18n("Last.fm is currently busy, please try again in a few minutes");
    case RateLimitExceeded: return i18n("Rate-limit exceeded");
    default:
        return msg.isEmpty() ? i18n("Unknown error") : msg.trimmed();
    }
}

static QString md5(const QString &s)
{
    return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5).toHex());
}

static void sign(QMap<QString, QString> &params)
{
    QString s;
    params[QLatin1String("api_key")] = Covers::constLastFmApiKey;
    QStringList keys=params.keys();
    keys.sort();
    foreach (const QString &k, keys) {
        s += k+params[k];
    }
    s += constSecretKey;
    params[QLatin1String("api_sig")] = md5(s);
}

static QByteArray format(const QMap<QString, QString> &params)
{
    QByteArray data;
    QMapIterator<QString, QString> i(params);
    while (i.hasNext()) {
        i.next();
        data+=QUrl::toPercentEncoding(i.key())+'='+QUrl::toPercentEncoding(i.value());
        if (i.hasNext()) {
            data+='&';
        }
    }
    DBUG_CLASS("Scrobber") << data;
    return data;
}

static QString cacheName(bool createDir)
{
    QString dir=Utils::cacheDir(Scrobbler::constCacheDir, createDir);
    return dir.isEmpty() ? QString() : (dir+Scrobbler::constCacheFile);
}

Scrobbler::Track::Track(const Song &s)
{
    title=s.title;
    artist=s.artist;
    album=s.album;
    albumartist=s.albumartist;
    track=s.track;
    length=s.time;
    timestamp=0;
}

Scrobbler::Scrobbler()
    : QObject(0)
    , scrobblingEnabled(false)
    , scrobbler("Last.fm")
    , nowPlayingIsPending(false)
    , lastScrobbleFailed(false)
    , failedCount(0)
    , authJob(0)
    , scrobbleJob(0)
{
    hardFailTimer = new QTimer(this);
    hardFailTimer->setInterval(60*1000);
    hardFailTimer->setSingleShot(true);
    scrobbleTimer = new PausableTimer();
    scrobbleTimer->setInterval(9000000); // huge number to avoid scrobbling just started song start (rare case)
    scrobbleTimer->setSingleShot(true);
    nowPlayingTimer = new PausableTimer();
    nowPlayingTimer->setSingleShot(true);
    nowPlayingTimer->setInterval(5000);
    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    retryTimer->setInterval(10000);
    connect(scrobbleTimer, SIGNAL(timeout()), this, SLOT(scrobbleCurrent()));
    connect(retryTimer, SIGNAL(timeout()), this, SLOT(scrobbleQueued()));
    connect(nowPlayingTimer, SIGNAL(timeout()), this, SLOT(sendNowPlaying()));
    connect(hardFailTimer, SIGNAL(timeout()), this, SLOT(authenticate()));
    loadSettings();
}

Scrobbler::~Scrobbler()
{
    stop();
}

void Scrobbler::stop()
{
    cancelJobs();
    saveCache();
}

void Scrobbler::setActive()
{
    if (isEnabled()) {
        loadCache();
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(mpdStateUpdated()));
        connect(MPDConnection::self(), SIGNAL(currentSongUpdated(Song)), this, SLOT(setSong(Song)));
    } else {
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(mpdStateUpdated()));
        disconnect(MPDConnection::self(), SIGNAL(currentSongUpdated(Song)), this, SLOT(setSong(Song)));
        songQueue.clear();
        lastScrobbledSongs.clear();
        saveCache();
        cancelJobs();
    }

    if (!isAuthenticated()) {
        authenticate();
    } else if (!songQueue.isEmpty()) {
        scrobbleQueued();
    }
}

void Scrobbler::loadSettings()
{
    Configuration cfg(constSettingsGroup);

    userName=cfg.get("userName", userName);
//    password=cfg.get("password", password);
    sessionKey=cfg.get("sessionKey", sessionKey);
    scrobblingEnabled=cfg.get("enabled", scrobblingEnabled);
    DBUG << userName << sessionKey << scrobblingEnabled;
    emit authenticated(isAuthenticated());
    emit enabled(isScrobblingEnabled());
    setActive();
}

void Scrobbler::setDetails(const QString &u, const QString &p)
{
    if (u!=userName || p!=password) {
        DBUG << "details changed";
        Configuration cfg(constSettingsGroup);

        userName=u;
        password=p;
        sessionKey=QString();
        cfg.set("userName", userName);
//        cfg.set("password", password);
        setActive();
        if (!isEnabled()) {
            emit authenticated(false);
        }
    } else if (!isAuthenticated() && haveLoginDetails()) {
        authenticate();
    } else {
        DBUG << "details NOT changed";
        emit authenticated(isAuthenticated());
    }
}

void Scrobbler::setEnabled(bool e)
{
    if (e!=scrobblingEnabled) {
        scrobblingEnabled=e;
        Configuration(constSettingsGroup).set("enabled", scrobblingEnabled);
        setActive();
        emit enabled(e);
    }
}

void Scrobbler::setSong(const Song &s)
{
    if (!isEnabled() || s.isStandardStream() || s.time<30) {
        return;
    }

    nowPlayingTimer->setInterval(5000);
    scrobbleTimer->setInterval(qMin(s.time/2, 240)*1000); // Scrobble at 1/2 way point or 4 mins - whichever comes first!
    if (currentSong.artist != s.artist || currentSong.title!=s.title || currentSong.album!=s.album) {
        currentSong=s;
        if (MPDState_Playing==MPDStatus::self()->state() && s.time>30) {
            mpdStateUpdated();
        }
    }
}

void Scrobbler::sendNowPlaying()
{
    nowPlayingIsPending = true;
    if (ensureAuthenticated()) {
        scrobbleNowPlaying(currentSong);
        nowPlayingIsPending = false;
    }
}

void Scrobbler::scrobbleNowPlaying(const Track &s)
{
    if (s.title.isEmpty() || s.artist.isEmpty()) {
        return;
    }
    QMap<QString, QString> params;
    params["method"] = "track.updateNowPlaying";
    params["track"] = s.title;
    if (!s.album.isEmpty()) {
        params["album"] = s.album;
    }
    params["artist"] = s.artist;
    if (!s.albumartist.isEmpty() && s.albumartist!=s.artist) {
        params["albumArtist"] = s.albumartist;
    }
    if (s.track) {
        params["trackNumber"] = QString::number(s.track);
    }
    if (s.length) {
        params["duration"] = QString::number(s.length);
    }
    params["sk"] = sessionKey;
    sign(params);
    DBUG << s.title << s.artist;
    QNetworkReply *job=NetworkAccessManager::self()->postFormData(QUrl(constUrl), format(params));
    connect(job, SIGNAL(finished()), this, SLOT(nowPlayingResp()));
}

void Scrobbler::nowPlayingResp()
{
    QNetworkReply *job=qobject_cast<QNetworkReply *>(sender());
    if (!job) {
        return;
    }
    job->deleteLater();
    DBUG << job->readAll();
}

void Scrobbler::scrobbleCurrent()
{
    songQueue.enqueue(currentSong);
    scrobbleQueued();
}

void Scrobbler::scrobbleQueued()
{
    if (!scrobblingEnabled) {
        return;
    }
    if (!ensureAuthenticated() || scrobbleJob) {
        if (!retryTimer->isActive()) {
            retryTimer->start();
        }
        return;
    }

    if (!songQueue.isEmpty()) {
        QMap<QString, QString> params;
        params["method"] = "track.scrobble";
        int batchSize=qMin(constMaxBatchSize, songQueue.size());
        DBUG << "trying scrobble " << batchSize << " songs";
        for (int i=0; i<batchSize; ++i) {
            Track s=songQueue.takeAt(0);

            params[QString("track[%1]").arg(i)] = s.title;
            if (!s.album.isEmpty()) {
                params[QString("album[%1]").arg(i)] = s.album;
            }
            params[QString("artist[%1]").arg(i)] = s.artist;
            if (!s.albumartist.isEmpty() && s.albumartist!=s.artist) {
                params[QString("albumArtist[%1]").arg(i)] = s.albumartist;
            }
            if (s.track) {
                params[QString("trackNumber[%1]").arg(i)] = QString::number(s.track);
            }
            if (s.length) {
                params[QString("duration[%1]").arg(i)] = QString::number(s.length);
            }
            params[QString("timestamp[%1]").arg(i)] = QString::number(s.timestamp);
            lastScrobbledSongs.append(s);
        }
        params["sk"] = sessionKey;
        sign(params);
        scrobbleJob=NetworkAccessManager::self()->postFormData(QUrl(constUrl), format(params));
        connect(scrobbleJob, SIGNAL(finished()), this, SLOT(scrobbleFinished()));
    }
}

void Scrobbler::scrobbleFinished()
{
    QNetworkReply *job=qobject_cast<QNetworkReply *>(sender());

    if (!job) {
        return;
    }
    job->deleteLater();
    if (job==scrobbleJob) {
        DBUG << job->errorString();
        scrobbleJob=0;
        QStringList data = QString(job->readAll()).split("\n");
        DBUG << data;
        if (data.isEmpty()) {
            return;
        }
        if (constOk==data[0]) {
            lastScrobbledSongs.clear();
        } else {
            songQueue << lastScrobbledSongs;
            lastScrobbledSongs.clear();
            handle(data[0]);
        }
    }
}

bool Scrobbler::ensureAuthenticated()
{
    if (!sessionKey.isEmpty()) {
        return true;
    }
    authenticate();
    return false;
}

void Scrobbler::authenticate()
{
    if (hardFailTimer->isActive() || authJob) {
        DBUG << "authentication delayed";
        return;
    }

    if (!haveLoginDetails()) {
        DBUG << "no login details";
        return;
    }
    QUrl url(constUrl);
    QMap<QString, QString> params;
    params["method"] = "auth.getMobileSession";
    params["username"] = userName;
    #ifdef ENABLE_HTTPS_SUPPORT
    params["password"] = password;
    url.setScheme("https"); // Use HTTPS to authenticate
    #else
    params["authToken"]=md5(userName+md5(password));
    #endif
    sign(params);

    authJob=NetworkAccessManager::self()->postFormData(url, format(params));
    connect(authJob, SIGNAL(finished()), this, SLOT(authResp()));
    DBUG << url.toString();
}

void Scrobbler::authResp()
{
    QNetworkReply *job=qobject_cast<QNetworkReply *>(sender());

    if (!job) {
        return;
    }
    job->deleteLater();
    if (job!=authJob) {
        return;
    }
    authJob=0;
    sessionKey.clear();

    QByteArray data=job->readAll();
    DBUG << data;
    QXmlStreamReader reader(data);
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement()) {
            QString element = reader.name().toString();
            if (QLatin1String("session")==element) {
                while (!reader.atEnd() && !reader.hasError()) {
                    reader.readNext();
                    if (reader.isStartElement()) {
                        element = reader.name().toString();
                        if (QLatin1String("key")==element) {
                            sessionKey = reader.readElementText();
                            break;
                        }
                    }
                }
                break;
            } else if (QLatin1String("error")==element) {
                int code=reader.attributes().value(QLatin1String("code")).toString().toInt();
                emit error(lastFmError(code, reader.readElementText()));
                break;
            }
        }
    }

    DBUG << "authenticated:" << !sessionKey.isEmpty();
    emit authenticated(isAuthenticated());
    Configuration cfg(constSettingsGroup);
    cfg.set("sessionKey", sessionKey);

    if (nowPlayingIsPending && isAuthenticated()) {
        scrobbleNowPlaying(currentSong);
        nowPlayingIsPending = false;
    }
}

void Scrobbler::handle(const QString &status)
{
    DBUG << status;
    if (constBadSession==status) {
        sessionKey.clear();
        authenticate();
        failedCount=0;
    } else if(status.startsWith(constFailed)) {
//        QStringList dat = status.split(" ");
//        if (dat.size()>1) {
//            emit error(i18n("%1 error: %1", scrobbler, dat.join(" ")));
//        }
        if (++failedCount > 2 && !hardFailTimer->isActive()) {
            sessionKey.clear();
            hardFailTimer->setInterval((failedCount > 120 ? 120 : failedCount)*60*1000);
            hardFailTimer->start();
        }
    } else {
        failedCount=0;
    }
}

void Scrobbler::loadCache()
{
    QString fileName=cacheName(false);
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (compressor.open(QIODevice::ReadOnly)) {
        QXmlStreamReader reader(&file);
        while(!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && QLatin1String("track")==reader.name().toString()) {
                Track t;
                t.artist = reader.attributes().value(QLatin1String("artist")).toString();
                t.album = reader.attributes().value(QLatin1String("album")).toString();
                t.albumartist = reader.attributes().value(QLatin1String("albumartist")).toString();
                t.title = reader.attributes().value(QLatin1String("title")).toString();
                t.track = reader.attributes().value(QLatin1String("track")).toString().toUInt();
                t.length = reader.attributes().value(QLatin1String("length")).toString().toUInt();
                t.timestamp = reader.attributes().value(QLatin1String("timestamp")).toString().toUInt();
                songQueue.append(t);
            }
        }
    }
}

void Scrobbler::saveCache()
{
    QString fileName=cacheName(true);
    DBUG << fileName << lastScrobbledSongs.count() << songQueue.count();
    if (fileName.isEmpty()) {
        return;
    }

    if (lastScrobbledSongs.isEmpty() && songQueue.isEmpty()) {
        if (QFile::exists(fileName)) {
            QFile::remove(fileName);
        }
        return;
    }

    QFile file(fileName);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (compressor.open(QIODevice::WriteOnly)) {
        QXmlStreamWriter writer(&compressor);
        writer.setAutoFormatting(false);
        writer.writeStartDocument();
        writer.writeStartElement("tracks");
        writer.writeAttribute("version", "1");

        const QQueue<Track> &queue=lastScrobbledSongs.isEmpty() ? songQueue : lastScrobbledSongs;
        foreach (const Track &t, queue) {
            writer.writeEmptyElement("track");
            writer.writeAttribute(QLatin1String("artist"), t.artist);
            writer.writeAttribute(QLatin1String("album"), t.album);
            writer.writeAttribute(QLatin1String("albumartist"), t.albumartist);
            writer.writeAttribute(QLatin1String("title"), t.title);
            writer.writeAttribute(QLatin1String("track"), QString::number(t.track));
            writer.writeAttribute(QLatin1String("length"), QString::number(t.length));
            writer.writeAttribute(QLatin1String("timestamp"), QString::number(t.timestamp));
        }
        writer.writeEndElement();
        writer.writeEndDocument();
    }
}

void Scrobbler::mpdStateUpdated()
{
    if (isEnabled()) {
        switch (MPDStatus::self()->state()) {
        case MPDState_Paused:
            scrobbleTimer->pause();
            nowPlayingTimer->pause();
            break;
        case MPDState_Playing:
            currentSong.timestamp = time(NULL);
            scrobbleTimer->start();
            nowPlayingTimer->start();
            break;
        default:
            scrobbleTimer->stop();
            nowPlayingTimer->stop();
            nowPlayingTimer->setInterval(5000);
        }
    }
}

void Scrobbler::cancelJobs()
{
    if (authJob) {
        disconnect(authJob, SIGNAL(finished()), this, SLOT(authResp()));
        authJob->close();
        authJob->abort();
        authJob->deleteLater();
        authJob=0;
    }
    if (scrobbleJob) {
        disconnect(scrobbleJob, SIGNAL(finished()), this, SLOT(scrobbleFinished()));
        authJob->close();
        authJob->abort();
        authJob->deleteLater();
        scrobbleJob=0;
    }
}
