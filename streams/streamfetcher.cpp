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

#include "streamfetcher.h"
#include "networkaccessmanager.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "streamsmodel.h"
#include "localize.h"
#include <QRegExp>
#include <QUrl>
#include <QXmlStreamReader>

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

static QString parsePlaylist(const QByteArray &data, const QString &key, const QSet<QString> &handlers)
{
    QStringList lines=QString(data).split('\n', QString::SkipEmptyParts);

    foreach (QString line, lines) {
        if (line.startsWith(key, Qt::CaseInsensitive)) {
            foreach (const QString &handler, handlers) {
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

    foreach (QString line, lines) {
        foreach (const QString &handler, handlers) {
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

    foreach (QString line, lines) {
        int ref=line.indexOf(QLatin1String("<ref href"), Qt::CaseInsensitive);
        if (-1!=ref) {
            foreach (const QString &handler, handlers) {
                QString protocol(handler+QLatin1String("://"));
                int prot=line.indexOf(protocol, Qt::CaseInsensitive);
                if (-1!=prot) {
                    QStringList parts=line.split('\"');
                    foreach (QString part, parts) {
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
            foreach (const QString &handler, handlers) {
                if (loc.startsWith(handler+QLatin1String("://"))) {
                    return loc;
                }
            }
        }
    }

    return QString();
}

static QString parse(const QByteArray &data)
{
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
    } else if (data.startsWith("http://")) {
        QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r)")), QString::SkipEmptyParts);
        if (!lines.isEmpty()) {
            DBUG << "http";
            return lines.first();
        }
    }

    return QString();
}

StreamFetcher::StreamFetcher(QObject *p)
    : QObject(p)
    , job(0)
{
}

StreamFetcher::~StreamFetcher()
{
}

void StreamFetcher::get(const QStringList &items, int insertRow, bool replace, quint8 priority)
{
    if (items.isEmpty()) {
        return;
    }

    DBUG << "get" << items;
    cancel();
    todo=items;
    done.clear();
    row=insertRow;
    replacePlayQueue=replace;
    prio=priority;
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
        emit status(i18n("Fetching %1").arg(currentName.isEmpty() ? u.toString() : currentName));
        if (!currentName.isEmpty()) {
            current=current.left(current.length()-(currentName.length()+1));
        }
        if (u.scheme().startsWith(StreamsModel::constPrefix)) {
            data.clear();
            u.setScheme("http");
            job=NetworkAccessManager::self()->get(u, constTimeout);
            DBUG << "Check" << u.toString();
            connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
            connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
            return;
        } else {
            DBUG << "use orig" << current;
            done.append(MPDParseUtils::addStreamName(current, currentName));
        }
    }

    if (todo.isEmpty() && !done.isEmpty()) {
        job=0;
        emit result(done, row, replacePlayQueue, prio);
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
    if (job) {
        disconnect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->abort();
        job->deleteLater();
    }
    job=0;
    emit status(QString());
}

void StreamFetcher::dataReady()
{
    data+=job->readAll();

    if (data.count()>constMaxData) {
        QNetworkReply *thisJob=job;
        disconnect(thisJob, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(thisJob, SIGNAL(finished()), this, SLOT(jobFinished()));
        jobFinished(thisJob);
        thisJob->abort();
        thisJob->deleteLater();
    }
}

void StreamFetcher::jobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        jobFinished(reply);
    }
}

void StreamFetcher::jobFinished(QNetworkReply *reply)
{
    // We only handle 1 job at a time!
    if (reply==job) {
        bool redirected=false;
        if (!reply->error()) {
            QVariant redirect = reply->header(QNetworkRequest::LocationHeader);
            if (redirect.isValid() && ++redirects<constMaxRedirects) {
                current=redirect.toString();
                DBUG << "real redirect" << current;
                data.clear();
                job=NetworkAccessManager::self()->get(current, constTimeout);
                connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                redirected=true;
            } else {
                QString u=parse(data);
                if (u.isEmpty() || u==current) {
                    DBUG << "use (empty/current)" << current;
                    done.append(MPDParseUtils::addStreamName(current.startsWith(StreamsModel::constPrefix) ? current.mid(StreamsModel::constPrefix.length()) : current, currentName));
                } else if (u.startsWith(QLatin1String("http://")) && ++redirects<constMaxRedirects) {
                    // Redirect...
                    current=u;
                    DBUG << "semi-redirect" << current;
                    data.clear();
                    job=NetworkAccessManager::self()->get(u, constTimeout);
                    connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                    redirected=true;
                } else {
                    DBUG << "use" << u;
                    done.append(MPDParseUtils::addStreamName(u, currentName));
                }
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
