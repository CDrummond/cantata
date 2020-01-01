/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "streamfetcher.h"
#include "network/networkaccessmanager.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "models/streamsmodel.h"
#include <QRegExp>
#include <QUrl>
#include <QXmlStreamReader>
#include <QJsonParseError>
#include <QJsonDocument>

#ifdef _MSC_VER 
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "StreamFetcher" << __FUNCTION__
void StreamFetcher::enableDebug()
{
    debugEnabled=true;
}

static const int constMaxRedirects = 3;
static const int constMaxData = 1024;
static const int constTimeout = 3*1000;
static const QString constTuneIn = QLatin1String("opml.radiotime.com");
static const QString constTuneInFmt = QLatin1String("render=json&formats=mp3,aac,ogg,hls");
static const QString constTuneInNotCompat = QLatin1String("service/Audio/notcompatible");

static QString parsePlaylist(const QByteArray &data, const QString &key, const QSet<QString> &handlers)
{
    QStringList lines=QString(data).split('\n', QString::SkipEmptyParts);

    for (QString line: lines) {
        if (line.startsWith(key, Qt::CaseInsensitive)) {
            for (const QString &handler: handlers) {
                QString protocol(handler+QLatin1String("://"));
                int index=line.indexOf(protocol, Qt::CaseInsensitive);
                if (index>-1 && index<7) {
                    line.remove('\n');
                    line.remove('\r');
                    return line.mid(index);
                }
            }
        }
    }

    return QString();
}

static QString parseExt3Mu(const QByteArray &data, const QSet<QString> &handlers)
{
    QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r)")), QString::SkipEmptyParts);

    for (QString line: lines) {
        for (const QString &handler: handlers) {
            QString protocol(handler+QLatin1String("://"));
            if (line.startsWith(protocol, Qt::CaseInsensitive)) {
                line.remove('\n');
                line.remove('\r');
                return line;
            }
        }
    }

    return QString();
}

static QString parseAsx(const QByteArray &data, const QSet<QString> &handlers)
{
    QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r|/>)")), QString::SkipEmptyParts);

    for (QString line: lines) {
        int ref=line.indexOf(QLatin1String("<ref href"), Qt::CaseInsensitive);
        if (-1!=ref) {
            for (const QString &handler: handlers) {
                QString protocol(handler+QLatin1String("://"));
                int prot=line.indexOf(protocol, Qt::CaseInsensitive);
                if (-1!=prot) {
                    QStringList parts=line.split('\"');
                    for (const QString &part: parts) {
                        if (part.startsWith(protocol)) {
                            return part;
                        }
                    }
                }
            }
        }
    }

    return QString();
}

static QString parseXml(const QByteArray &data, const QSet<QString> &handlers)
{
    // XSPF / SPIFF
    QXmlStreamReader reader(data);

    while (!reader.atEnd()) {
        reader.readNext();
        if (QXmlStreamReader::StartElement==reader.tokenType() && QLatin1String("location")==reader.name()) {
            QString loc=reader.readElementText().trimmed();
            for (const QString &handler: handlers) {
                if (loc.startsWith(handler+QLatin1String("://"))) {
                    return loc;
                }
            }
        }
    }

    return QString();
}

static QString parse(const QByteArray &data, const QString &host)
{
    DBUG << host;
    if (host==constTuneIn) {
        QJsonParseError jsonParseError;
        QVariantMap parsed=QJsonDocument::fromJson(data, &jsonParseError).toVariant().toMap();
        bool ok=QJsonParseError::NoError==jsonParseError.error;

        if (ok && !parsed.isEmpty() && parsed.contains("body")) {
            QVariantList body = parsed["body"].toList();
            for (const auto &e: body) {
                QVariantMap em = e.toMap();

                if (em.contains("url")) {
                    return em["url"].toString();
                }
            }
        }
    }
    QSet<QString> handlers=MPDConnection::self()->urlHandlers();
    if (data.length()>10 && !strncasecmp(data.constData(), "[playlist]", 10)) {
        DBUG << "playlist";
        return parsePlaylist(data, QLatin1String("File"), handlers);
    } else if (data.length()>7 && (!strncasecmp(data.constData(), "#EXTM3U", 7) || !strncasecmp(data.constData(), "http://", 7))) {
        DBUG << "ext3mu";
        return parseExt3Mu(data, handlers);
    } else if (data.length()>5 && !strncasecmp(data.constData(), "<asx ", 5)) {
        DBUG << "asx";
        return parseAsx(data, handlers);
    } else if (data.length()>11 && !strncasecmp(data.constData(), "[reference]", 11)) {
        DBUG << "playlist/ref";
        return parsePlaylist(data, QLatin1String("Ref"), handlers);
    } else if (data.length()>5 && !strncasecmp(data.constData(), "<?xml", 5)) {
        DBUG << "xml";
        return parseXml(data, handlers);
    } else if ( (-1==data.indexOf("<html") && -1!=data.indexOf("http:/")) || // flat list?
                (-1!=data.indexOf("#EXTM3U")) ) { // m3u with comments?
        DBUG << "ext3mu/2";
        return parseExt3Mu(data, handlers);
    } else {
        for (const auto &h: handlers) {
            DBUG << h;
            if (data.startsWith(h.toLatin1()+"://")) {
                QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r)")), QString::SkipEmptyParts);
                if (!lines.isEmpty()) {
                    DBUG << h;
                    return lines.first();
                }
            }
        }
    }

    return QString();
}

StreamFetcher::StreamFetcher(QObject *p)
    : QObject(p)
    , job(nullptr)
    , row(0)
    , playQueueAction(true)
    , prio(0)
    , redirects(0)
{
}

StreamFetcher::~StreamFetcher()
{
}

void StreamFetcher::get(const QStringList &items, int insertRow, int action, quint8 priority, bool decPriority)
{
    if (items.isEmpty()) {
        return;
    }

    DBUG << "get" << items;
    cancel();
    todo=items;
    done.clear();
    row=insertRow;
    playQueueAction=action;
    prio=priority;
    decreasePriority=decPriority;
    current=QString();
    currentName=QString();
    doNext();
}

void StreamFetcher::doNext()
{
    redirects=0;
    while (todo.count()) {
        current=todo.takeFirst();
        QUrl u(current);
        currentName=MPDParseUtils::getStreamName(current);
        QString report=currentName;
        if (report.isEmpty()) {
            report=u.toString();
            if (report.startsWith(StreamsModel::constPrefix)) {
                report=report.mid(StreamsModel::constPrefix.length());
            }
        }
        emit status(tr("Loading %1").arg(report));
        current=Utils::removeHash(current);
        // MPD 0.19.2 can handle m3u8
        if (u.path().endsWith(QLatin1String(".m3u8"))) {
            DBUG << "use orig (m3u8)" << current;
            done.append(MPDParseUtils::addStreamName(current.startsWith(StreamsModel::constPrefix) ? current.mid(StreamsModel::constPrefix.length()) : current, currentName));
        } else if (u.scheme().startsWith(StreamsModel::constPrefix)) {
            data.clear();
            u.setScheme("http");
            if (u.host()==constTuneIn) {
                QString query=u.query();
                query+=query.isEmpty() ? "?" : "&";
                u.setQuery(query+constTuneInFmt);
            }

            job=NetworkAccessManager::self()->get(u, constTimeout);
            DBUG << "Check" << u.toString() << static_cast<void*>(job);
            connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
            connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
            return;
        } else {
            DBUG << "use orig" << current;
            done.append(MPDParseUtils::addStreamName(current, currentName));
        }
    }

    if (todo.isEmpty() && !done.isEmpty()) {
        job=nullptr;
        emit result(done, row, playQueueAction, prio, decreasePriority);
        emit status(QString());
    }
}

void StreamFetcher::cancel()
{
    todo.clear();
    done.clear();
    row=0;
    data.clear();
    current=QString();
    cancelJob();
    emit status(QString());
}

void StreamFetcher::dataReady()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (reply!=job) {
        return;
    }

    data+=job->readAll();

    if (data.count()>constMaxData) {
        NetworkJob *thisJob=job;
        jobFinished(thisJob);
        // If jobFinished did not redirect, then we need to ensure job is cancelled.
        if (thisJob==job) {
            cancelJob();
        }
    }
}

void StreamFetcher::jobFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (reply) {
        jobFinished(reply);
    }
}

void StreamFetcher::jobFinished(NetworkJob *reply)
{
    // We only handle 1 job at a time!
    DBUG << static_cast<void*>(reply);
    if (reply==job) {
        disconnect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        bool redirected=false;
        if (!reply->error()) {
            QString u=parse(data, reply->origUrl().host());
            QUrl orig = reply->origUrl();

            if (u.isEmpty() || u==current || (orig.host()==constTuneIn && u.contains(constTuneInNotCompat))) {
                QString query = orig.query();
                if (orig.host()==constTuneIn && query.contains(constTuneInFmt)) {
                    // With formats failed (Issue #1491), so try without
                    query = query.replace(constTuneInFmt, "");
                    if (query.endsWith("?") || query.endsWith("&")) {
                        query = query.left(query.length()-1);
                    }
                    orig.setQuery(query);
                    job=NetworkAccessManager::self()->get(orig, constTimeout);
                    DBUG << "Check plain" << orig.toString() << static_cast<void*>(job);
                    connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                    redirected=true;
                    data.clear();
                } else {
                    DBUG << "use (empty/current)" << current;
                    done.append(MPDParseUtils::addStreamName(current.startsWith(StreamsModel::constPrefix) ? current.mid(StreamsModel::constPrefix.length()) : current, currentName));
                }

            } else if (u.startsWith(QLatin1String("http://")) && ++redirects<constMaxRedirects) {
                // Redirect...
                current=u;
                DBUG << "semi-redirect" << current;
                data.clear();
                cancelJob();
                job=NetworkAccessManager::self()->get(u, constTimeout);
                connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                redirected=true;
            } else {
                DBUG << "use" << u;
                done.append(MPDParseUtils::addStreamName(u, currentName));
            }
        } else {
            DBUG << "error " << reply->errorString() << " - use" << current;
            done.append(MPDParseUtils::addStreamName(current.startsWith(StreamsModel::constPrefix) ? current.mid(StreamsModel::constPrefix.length()) : current, currentName));
        }

        if (!redirected) {
            doNext();
        }
    }
    reply->deleteLater();
}

void StreamFetcher::cancelJob()
{
    if (job) {
        disconnect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->cancelAndDelete();
        job=nullptr;
    }
}

#include "moc_streamfetcher.cpp"
