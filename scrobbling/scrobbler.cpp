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

#include "scrobbler.h"
#include "pausabletimer.h"
#include "config.h"
#include "gui/apikeys.h"
#include "network/networkaccessmanager.h"
#include "mpd-interface/mpdconnection.h"
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
#include <QSslSocket>

#include <QDebug>
static bool debugIsEnabled=false;
static bool fakeScrobbling=false;
#define DBUG if (debugIsEnabled) qWarning() << metaObject()->className() << __FUNCTION__
#define DBUG_CLASS(C) if (debugIsEnabled) qWarning() << C << __FUNCTION__
void Scrobbler::enableDebug()
{
    debugIsEnabled=true;
}

const QLatin1String Scrobbler::constCacheDir("scrobbling");
const QLatin1String Scrobbler::constCacheFile("tracks.xml.gz");
static const QLatin1String constSettingsGroup("Scrobbling");
static const QString constSecretKey=QLatin1String("0753a75ccded9b17b872744d4bb60b35");
static const int constMaxBatchSize=50;
static const int constNowPlayingInterval=5000;

GLOBAL_STATIC(Scrobbler, instance)

enum LastFmErrors
{
    NoError = 0, // ??

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

static QString errorString(int code, const QString &msg)
{
    switch (code) {
    case InvalidService: return QObject::tr("Invalid service");
    case InvalidMethod: return QObject::tr("Invalid method");
    case AuthenticationFailed: return QObject::tr("Authentication failed");
    case InvalidFormat: return QObject::tr("Invalid format");
    case InvalidParameters: return QObject::tr("Invalid parameters");
    case InvalidResourceSpecified: return QObject::tr("Invalid resource specified");
    case OperationFailed: return QObject::tr("Operation failed");
    case InvalidSessionKey: return QObject::tr("Invalid session key");
    case InvalidApiKey: return QObject::tr("Invalid API key");
    case ServiceOffline: return QObject::tr("Service offline");
    case TryAgainLater: return QObject::tr("Last.fm is currently busy, please try again in a few minutes");
    case RateLimitExceeded: return QObject::tr("Rate-limit exceeded");
    default:
        return msg.isEmpty() ? QObject::tr("Unknown error") : msg.trimmed();
    }
}

static QString md5(const QString &s)
{
    return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Md5).toHex());
}

static void sign(QMap<QString, QString> &params)
{
    QString s;
    params[QLatin1String("api_key")] = ApiKeys::self()->get(ApiKeys::LastFm);
    QStringList keys=params.keys();
    keys.sort();
    for (const QString &k: keys) {
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
        data+=i.key().toLatin1()+'='+QUrl::toPercentEncoding(i.value());
        if (i.hasNext()) {
            data+='&';
        }
    }
    DBUG_CLASS("Scrobbler") << data;
    return data;
}

static QString cacheName(bool createDir)
{
    QString dir=Utils::cacheDir(Scrobbler::constCacheDir, createDir);
    return dir.isEmpty() ? QString() : (dir+Scrobbler::constCacheFile);
}

Scrobbler::Track::Track(const Song &s)
{
    if (!(s.blank&Song::BlankTitle)) {
        title=s.title;
    }
    if (!(s.blank&Song::BlankArtist)) {
        artist=s.artist;
    }
    if (!(s.blank&Song::BlankAlbum)) {
        album=s.album;
    }
    albumartist=s.albumartist;
    track=s.track;
    length=s.time;
    timestamp=0;
}

Scrobbler::Scrobbler()
    : QObject(nullptr)
    , scrobblingEnabled(false)
    , loveIsEnabled(false)
    , scrobbler("Last.fm")
    , lastNowPlaying(0)
    , nowPlayingIsPending(false)
    , lovePending(false)
    , lastScrobbleFailed(false)
    , nowPlayingSent(false)
    , loveSent(false)
    , scrobbledCurrent(false)
    , scrobbleViaMpd(false)
    , failedCount(0)
    , lastState(MPDState_Inactive)
    , authJob(nullptr)
    , scrobbleJob(nullptr)
{
    hardFailTimer = new QTimer(this);
    hardFailTimer->setInterval(60*1000);
    hardFailTimer->setSingleShot(true);
    scrobbleTimer = new PausableTimer();
    scrobbleTimer->setInterval(9000000); // fakeScrobblinghuge number to avoid scrobbling just started song start (rare case)
    scrobbleTimer->setSingleShot(true);
    nowPlayingTimer = new PausableTimer();
    nowPlayingTimer->setSingleShot(true);
    nowPlayingTimer->setInterval(constNowPlayingInterval);
    retryTimer = new QTimer(this);
    retryTimer->setSingleShot(true);
    retryTimer->setInterval(10000);
    connect(scrobbleTimer, SIGNAL(timeout()), this, SLOT(scrobbleCurrent()));
    connect(retryTimer, SIGNAL(timeout()), this, SLOT(scrobbleQueued()));
    connect(nowPlayingTimer, SIGNAL(timeout()), this, SLOT(scrobbleNowPlaying()));
    connect(hardFailTimer, SIGNAL(timeout()), this, SLOT(authenticate()));
    loadSettings();
    connect(this, SIGNAL(clientMessage(QString,QString,QString)), MPDConnection::self(), SLOT(sendClientMessage(QString,QString,QString)));
    connect(MPDConnection::self(), SIGNAL(clientMessageFailed(QString,QString)), SLOT(clientMessageFailed(QString,QString)));
    connect(MPDConnection::self(), SIGNAL(statusUpdated(MPDStatusValues)), this, SLOT(mpdStatusUpdated(MPDStatusValues)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(Song)), this, SLOT(setSong(Song)));
}

Scrobbler::~Scrobbler()
{
    DBUG;
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
    } else {
        reset();
    }

    if (isEnabled() || scrobbleViaMpd) {
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(mpdStateUpdated()));
    } else {
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(mpdStateUpdated()));
    }

    if (isEnabled() && !inactiveSong.isEmpty()) {
        Song s;
        s.title=inactiveSong.title;
        s.artist=inactiveSong.artist;
        s.albumartist=inactiveSong.albumartist;
        s.album=inactiveSong.album;
        s.track=inactiveSong.track;
        s.time=inactiveSong.length;
        setSong(s);
        inactiveSong.clear();
    }
    if (!isAuthenticated()) {
        authenticate();
    } else if (!songQueue.isEmpty()) {
        scrobbleQueued();
    }
}

static const QLatin1String constFakeScrobbling("fake");

void Scrobbler::loadSettings()
{
    Configuration cfg(constSettingsGroup);

    userName=cfg.get("userName", userName);
//    password=cfg.get("password", password);
    sessionKey=cfg.get("sessionKey", sessionKey);
    scrobblingEnabled=cfg.get("enabled", scrobblingEnabled);
    loveIsEnabled=cfg.get("loveEnabled", loveIsEnabled);
    scrobbler=cfg.get("scrobbler", scrobbler);
    scrobbleViaMpd=viaMpd(scrobblerUrl());
    fakeScrobbling=constFakeScrobbling==scrobbler;
    if (fakeScrobbling) {
        sessionKey=constFakeScrobbling;
    }
    DBUG << scrobbler << userName << sessionKey.isEmpty() << scrobblingEnabled;
    emit authenticated(isAuthenticated());
    emit enabled(isEnabled());
    emit loveEnabled(loveIsEnabled);
    setActive();
}

void Scrobbler::setDetails(const QString &s, const QString &u, const QString &p)
{
    if (fakeScrobbling) {
        return;
    }

    if (u!=scrobbler || u!=userName || p!=password) {
        DBUG << "details changed";
        Configuration cfg(constSettingsGroup);

        scrobbler=s;
        userName=u;
        password=p;
        sessionKey=QString();
        reset();
        cfg.set("scrobbler", scrobbler);
        cfg.set("userName", userName);
        cfg.set("sessionKey", sessionKey);
        scrobbleViaMpd=viaMpd(scrobblerUrl());
//        cfg.set("password", password);
        setActive();
        if (!isEnabled()) {
            emit authenticated(false);
        }
        emit scrobblerChanged();
    } else if (!isAuthenticated() && haveLoginDetails()) {
        authenticate();
    } else if (!scrobbleViaMpd) {
        DBUG << "details NOT changed";
        emit authenticated(isAuthenticated());
    }
}

void Scrobbler::love()
{
    if (lovedTrack()) {
        return;
    }

    lovePending=false;
    if (!loveIsEnabled) {
        return;
    }

    const Track &song=isEnabled() ? currentSong : inactiveSong;
    if (song.title.isEmpty() || song.artist.isEmpty() || loveSent) {
        return;
    }

    if (scrobbleViaMpd) {
        emit clientMessage(scrobblerUrl(), QLatin1String("love"), scrobbler);
        loveSent=true;
        return;
    }

    if (!ensureAuthenticated()) {
        lovePending=true;
        return;
    }

    QMap<QString, QString> params;
    params["method"] = "track.love";
    params["track"] = song.title;
    params["artist"] = song.artist;
    params["sk"] = sessionKey;
    sign(params);
    DBUG << song.title << song.artist;
    loveSent=true;
    if (fakeScrobbling) {
        DBUG << "MSG" << params;
    } else {
        QNetworkReply *job=NetworkAccessManager::self()->postFormData(QUrl(scrobblerUrl()), format(params));
        connect(job, SIGNAL(finished()), this, SLOT(handleResp()));
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

void Scrobbler::setLoveEnabled(bool e)
{
    if (e!=loveIsEnabled) {
        loveIsEnabled=e;
        Configuration(constSettingsGroup).set("loveEnabled", loveIsEnabled);
        setActive();
        emit loveEnabled(e);
    }
}

void Scrobbler::calcScrobbleIntervals()
{
    int elapsed=MPDStatus::self()->timeElapsed()*1000;
    if (elapsed<0) {
        elapsed=0;
    }
    int nowPlayingTimemout=constNowPlayingInterval;
    if (elapsed>4000) {
        nowPlayingTimemout=10;
    } else {
        nowPlayingTimemout-=elapsed;
    }
    nowPlayingTimer->setInterval(nowPlayingTimemout);
    int timeout=qMin(currentSong.length/2, (quint32)240)*1000; // Scrobble at 1/2 way point or 4 mins - whichever comes first!
    DBUG << "timeout" << timeout << elapsed << nowPlayingTimemout;
    if (timeout>elapsed) {
        timeout-=elapsed;
    } else {
        timeout=100;
    }
    scrobbleTimer->setInterval(timeout);
}

void Scrobbler::setSong(const Song &s)
{
    DBUG << isEnabled() << s.isStandardStream() << s.time << s.file << s.title << s.artist << s.album << s.albumartist << s.blank;
    if (!scrobbleViaMpd && !isEnabled()) {
        if (inactiveSong.artist != s.artist || inactiveSong.title!=s.title || inactiveSong.album!=s.album) {
            emit songChanged(!s.isStandardStream() && !s.isEmpty());
        }
        inactiveSong=Track(s);
        return;
    }

    inactiveSong.clear();
    if (currentSong.artist != s.artist || currentSong.title!=s.title || currentSong.album!=s.album) {
        nowPlayingSent=scrobbledCurrent=loveSent=lovePending=nowPlayingIsPending=false;
        currentSong=Track(s);
        lastNowPlaying=0;
        emit songChanged(!s.isStandardStream() && !s.isEmpty());
        if (scrobbleViaMpd || !isEnabled() || s.isStandardStream() || s.time<30) {
            return;
        }

        calcScrobbleIntervals();
        if (MPDState_Playing==MPDStatus::self()->state()) {
            mpdStateUpdated(true);
        }
    }
}

void Scrobbler::scrobbleNowPlaying()
{
    nowPlayingIsPending=false;
    if (!ensureAuthenticated()) {
        nowPlayingIsPending=true;
        return;
    }
    if (currentSong.title.isEmpty() || currentSong.artist.isEmpty() || nowPlayingSent || scrobbleViaMpd) {
        return;
    }
    QMap<QString, QString> params;
    params["method"] = "track.updateNowPlaying";
    params["track"] = currentSong.title;
    if (!currentSong.album.isEmpty()) {
        params["album"] = currentSong.album;
    }
    params["artist"] = currentSong.artist;
    if (!currentSong.albumartist.isEmpty() && currentSong.albumartist!=currentSong.artist) {
        params["albumArtist"] = currentSong.albumartist;
    }
    if (currentSong.track) {
        params["trackNumber"] = QString::number(currentSong.track);
    }
    if (currentSong.length) {
        params["duration"] = QString::number(currentSong.length);
    }
    params["sk"] = sessionKey;
    sign(params);
    DBUG << currentSong.title << currentSong.artist << currentSong.albumartist << currentSong.album << currentSong.track << currentSong.length;
    nowPlayingSent=true;
    lastNowPlaying=time(nullptr);
    if (fakeScrobbling) {
        DBUG << "MSG" << params;
    } else {
        QNetworkReply *job=NetworkAccessManager::self()->postFormData(QUrl(scrobblerUrl()), format(params));
        connect(job, SIGNAL(finished()), this, SLOT(handleResp()));
    }
}

void Scrobbler::handleResp()
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
    if (!scrobbledCurrent) {
        if (songQueue.isEmpty() || songQueue.last()!=currentSong) {
            songQueue.enqueue(currentSong);
        }
        scrobbledCurrent=true;
    }
    scrobbleQueued();
}

void Scrobbler::scrobbleQueued()
{
    if (!scrobblingEnabled || scrobbleViaMpd) {
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
        DBUG << "queued:" << songQueue.size() << "batchSize:" << batchSize;
        for (int i=0; i<batchSize; ++i) {
            Track s=songQueue.takeAt(0);
            DBUG << s.artist << s.albumartist << s.album << s.title << s.track << s.length << s.timestamp;
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
        if (fakeScrobbling) {
            DBUG << "MSG" << params;
            lastScrobbledSongs.clear();
        } else {
            scrobbleJob=NetworkAccessManager::self()->postFormData(scrobblerUrl(), format(params));
            connect(scrobbleJob, SIGNAL(finished()), this, SLOT(scrobbleFinished()));
        }
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
        QByteArray data=job->readAll();
        DBUG << job->errorString() << data << songQueue.size() << lastScrobbledSongs.size();
        scrobbleJob=nullptr;

        int errorCode=NoError;
        QXmlStreamReader reader(data);
        while (!reader.atEnd() && !reader.hasError()) {
            reader.readNext();
            if (reader.isStartElement()) {
                if (QLatin1String("lfm")==reader.name().toString()) {
                    QString status=reader.attributes().value("status").toString().toLower();
                    DBUG << status;
                    if (QLatin1String("failed")==status) {
                        while (!reader.atEnd() && !reader.hasError()) {
                            reader.readNext();
                            if (reader.isStartElement()) {
                                if (QLatin1String("error")==reader.name().toString()) {
                                    errorCode=reader.attributes().value(QLatin1String("code")).toString().toInt();
                                    QString errorStr=errorString(errorCode, reader.readElementText());
                                    emit error(tr("%1 error: %2").arg(scrobbler).arg(errorStr));
                                    DBUG << errorStr;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        switch (errorCode) {
        case NoError:
            failedCount=0;
            DBUG << "Scrobble succeeded";
            lastScrobbledSongs.clear();
            return;
        case AuthenticationFailed:
        case InvalidSessionKey:
        case TokenNotAuthorised:
            sessionKey.clear();
            authenticate();
            failedCount=0;
            break;
        default:
            if (++failedCount > 2 && !hardFailTimer->isActive()) {
                sessionKey.clear();
                hardFailTimer->setInterval((failedCount > 120 ? 120 : failedCount)*60*1000);
                hardFailTimer->start();
            }
            break;
        }

        DBUG << "Move last scrobbled into queued";
        songQueue << lastScrobbledSongs;
        lastScrobbledSongs.clear();
    }
}

bool Scrobbler::ensureAuthenticated()
{
    if (fakeScrobbling || !sessionKey.isEmpty()) {
        return true;
    }
    authenticate();
    return false;
}

void Scrobbler::authenticate()
{
    if (fakeScrobbling) {
        return;
    }
    if (hardFailTimer->isActive() || authJob || scrobbleViaMpd) {
        DBUG << "authentication delayed";
        return;
    }

    if (!haveLoginDetails()) {
        DBUG << "no login details";
        return;
    }
    QUrl url(scrobblerUrl());
    QMap<QString, QString> params;
    params["method"] = "auth.getMobileSession";
    params["username"] = userName;

    bool supportsSsl = false;
    #ifndef QT_NO_SSL
    supportsSsl = QSslSocket::supportsSsl();
    #endif
    if (supportsSsl) {
        params["password"] = password;
        url.setScheme("https"); // Use HTTPS to authenticate
    } else {
        params["authToken"]=md5(userName+md5(password));
    }
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
    authJob=nullptr;
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
                emit error(tr("%1 error: %2").arg(scrobbler).arg(errorString(code, reader.readElementText())));
                break;
            }
        }
    }

    DBUG << "authenticated:" << !sessionKey.isEmpty();
    emit authenticated(isAuthenticated());
    Configuration cfg(constSettingsGroup);
    cfg.set("sessionKey", sessionKey);

    if (isAuthenticated()) {
        if (nowPlayingIsPending) {
            scrobbleNowPlaying();
        }
        if (lovePending) {
            love();
        }
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
        QXmlStreamReader reader(&compressor);
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && QLatin1String("track")==reader.name()) {
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
    DBUG << fileName << songQueue.size();
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
        for (const Track &t: queue) {
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

void Scrobbler::mpdStateUpdated(bool songChanged)
{
    if (isEnabled() && !scrobbleViaMpd) {
        DBUG << songChanged << lastState << MPDStatus::self()->state();
        bool stateChange=songChanged || lastState!=MPDStatus::self()->state();
        bool isRepeat=!stateChange && MPDState_Playing==MPDStatus::self()->state() &&
                      MPDStatus::self()->timeElapsed()>=0 && MPDStatus::self()->timeElapsed()<2;
        if (!stateChange && !isRepeat) {
            return;
        }
        lastState=MPDStatus::self()->state();
        switch (lastState) {
        case MPDState_Paused:
            scrobbleTimer->pause();
            nowPlayingTimer->pause();
            break;
        case MPDState_Playing: {
            time_t now=time(nullptr);
            currentSong.timestamp = now-MPDStatus::self()->timeElapsed();
            DBUG << "Timestamp:" << currentSong.timestamp << "scrobbledCurrent:" << scrobbledCurrent << "nowPlayingSent:" << nowPlayingSent
                 << "now:" << now << "lastNowPlaying:" << lastNowPlaying << "isRepeat:" << isRepeat;

            if (isRepeat) {
                calcScrobbleIntervals();
                nowPlayingSent=scrobbledCurrent=nowPlayingIsPending=false;
                lastNowPlaying=0;
                scrobbleTimer->start();
                nowPlayingTimer->start();
                return;
            }
            if (!scrobbledCurrent) {
                scrobbleTimer->start();
            }
            // Send now playing if it has not already been sent, or if not scrobbled current track and its been
            // over X seconds since now paying was sent.
            if (!nowPlayingSent) {
                nowPlayingTimer->start();
            } else if (!scrobbledCurrent && ((now-lastNowPlaying)*1000)>constNowPlayingInterval) {
                int remaining=(MPDStatus::self()->timeTotal()-MPDStatus::self()->timeElapsed())*1000;
                DBUG << "remaining:" << remaining;
                if (remaining>constNowPlayingInterval) {
                    nowPlayingTimer->setInterval(constNowPlayingInterval);
                    nowPlayingSent=false;
                    nowPlayingTimer->start();
                }
            }
            break;
        }
        default:
            scrobbleTimer->stop();
            nowPlayingTimer->stop();
            nowPlayingTimer->setInterval(constNowPlayingInterval);
        }
    }
}

void Scrobbler::mpdStatusUpdated(const MPDStatusValues &vals)
{
    if (!vals.playlistLength) {
        currentSong.clear();
        emit songChanged(false);
    }
}

void Scrobbler::clientMessageFailed(const QString &client, const QString &msg)
{
    if (loveSent && client==scrobblerUrl() && msg==QLatin1String("love")) {
        // 'love' failed, so re-enable...
        loveSent=lovePending=false;
        emit songChanged(true);
    }
}

void Scrobbler::cancelJobs()
{
    if (authJob) {
        disconnect(authJob, SIGNAL(finished()), this, SLOT(authResp()));
        authJob->close();
        authJob->abort();
        authJob->deleteLater();
        authJob=nullptr;
    }
    if (scrobbleJob) {
        disconnect(scrobbleJob, SIGNAL(finished()), this, SLOT(scrobbleFinished()));
        authJob->close();
        authJob->abort();
        authJob->deleteLater();
        scrobbleJob=nullptr;
    }
}

void Scrobbler::reset()
{
    songQueue.clear();
    lastScrobbledSongs.clear();
    saveCache();
    cancelJobs();
}

void Scrobbler::loadScrobblers()
{
    if (scrobblers.isEmpty()) {
        QStringList files;
        QString userDir=Utils::dataDir();

        if (!userDir.isEmpty()) {
            files.append(Utils::fixPath(userDir)+QLatin1String("scrobblers.xml"));
        }

        files.append(":scrobblers.xml");

        for (const auto &f: files) {
            QFile file(f);
            if (file.open(QIODevice::ReadOnly)) {
                QXmlStreamReader doc(&file);
                while (!doc.atEnd()) {
                    doc.readNext();
                    if (doc.isStartElement() && QLatin1String("scrobbler")==doc.name()) {
                        QString name=doc.attributes().value("name").toString();
                        QString url=doc.attributes().value("url").toString();
                        if (!name.isEmpty() && !url.isEmpty() && !scrobblers.contains(name)) {
                            scrobblers.insert(name, url);
                        }
                    }
                }
            }
        }
    }
}

QString Scrobbler::scrobblerUrl()
{
    loadScrobblers();
    if (scrobblers.isEmpty()) {
        return QString();
    }

    if (!scrobblers.contains(scrobbler)) {
        return scrobblers.constBegin().value();
    }
    return scrobblers[scrobbler];
}

#include "moc_scrobbler.cpp"
