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

#include "streamfetcher.h"
#include "networkaccessmanager.h"
#include "mpdconnection.h"
#include <QtCore/QRegExp>
#include <QtCore/QUrl>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

static const int constMaxRedirects = 3;
static const int constMaxData = 12 * 1024;

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
        int ref=line.indexOf(QLatin1String("<ref href="), Qt::CaseInsensitive);
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
    QDomDocument doc;
    doc.setContent(data);
    QDomElement root = doc.documentElement();

    if (QLatin1String("playlist") == root.tagName()) {
        QDomElement tl = root.firstChildElement(QLatin1String("trackList"));
        while (!tl.isNull()) {
            QDomElement t = root.firstChildElement(QLatin1String("track"));
            while (!t.isNull()) {
                QDomElement l = root.firstChildElement(QLatin1String("location"));
                while (!l.isNull()) {
                    QString node=l.nodeValue();
                    foreach (const QString &handler, handlers) {
                        if (node.startsWith(handler+QLatin1String("://"))) {
                            return node;
                        }
                    }
                    l=l.nextSiblingElement(QLatin1String("location"));
                }
                t=t.nextSiblingElement(QLatin1String("track"));
            }
            tl=tl.nextSiblingElement(QLatin1String("trackList"));
        }
    }

    return QString();
}

static QString parse(const QByteArray &data)
{
    QSet<QString> handlers=MPDConnection::self()->urlHandlers();
    if (data.length()>10 && !strncasecmp(data.constData(), "[playlist]", 10)) {
        return parsePlaylist(data, QLatin1String("File"), handlers);
    } else if (data.length()>7 && (!strncasecmp(data.constData(), "#EXTM3U", 7) || !strncasecmp(data.constData(), "http://", 7))) {
        return parseExt3Mu(data, handlers);
    } else if (data.length()>5 && !strncasecmp(data.constData(), "<asx ", 5)) {
        return parseAsx(data, handlers);
    } else if (data.length()>11 && !strncasecmp(data.constData(), "[reference]", 11)) {
        return parsePlaylist(data, QLatin1String("Ref"), handlers);
    } else if (data.length()>5 && !strncasecmp(data.constData(), "<?xml", 5)) {
        return parseXml(data, handlers);
    } else if ( (-1==data.indexOf("<html") && -1!=data.indexOf("http:/")) || // flat list?
                (-1!=data.indexOf("#EXTM3U")) ) { // m3u with comments?
        return parseExt3Mu(data, handlers);
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

    cancel();
    todo=items;
    done.clear();
    row=insertRow;
    replacePlayQueue=replace;
    prio=priority;
    current=QString();
    doNext();
}

void StreamFetcher::doNext()
{
    redirects=0;
    while (todo.count()) {
        current=todo.takeFirst();
        QUrl u(current);

        if (QLatin1String("cantata-http")==u.scheme()) {
            data.clear();
            u.setScheme("http");
            job=NetworkAccessManager::self()->get(u);
            connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
            connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
            return;
        } else {
            done.append(current);
        }
    }

    if (todo.isEmpty() && !done.isEmpty()) {
        job=0;
        emit result(done, row, replacePlayQueue, prio);
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
                data.clear();
                job=NetworkAccessManager::self()->get(current);
                connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                redirected=true;
            } else {
                QString u=parse(data);

                if (u.isEmpty() || u==current) {
                    done.append(current.startsWith("cantata-http:") ? current.mid(8) : current);
                } else if (u.startsWith(QLatin1String("http://")) && ++redirects<constMaxRedirects) {
                    // Redirect...
                    current=u;
                    data.clear();
                    job=NetworkAccessManager::self()->get(u);
                    connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
                    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                    redirected=true;
                } else {
                    done.append(u);
                }
            }
        } else {
            done.append(current.startsWith("cantata-http:") ? current.mid(8) : current);
        }

        if (!redirected) {
            doNext();
        }
    }
    reply->deleteLater();
}
