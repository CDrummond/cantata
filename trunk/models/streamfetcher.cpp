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
#include <QtCore/QRegExp>
#include <QtCore/QUrl>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

static const int constMaxRedirects = 3;
static const int constMaxData = 12 * 1024;

static QString parsePlaylist(const QByteArray &data, const QString &key)
{
    QStringList lines=QString(data).split('\n', QString::SkipEmptyParts);

    foreach (QString line, lines) {
        if (line.startsWith(key, Qt::CaseInsensitive)) {
            int index=line.indexOf(QLatin1String("=http://"), Qt::CaseInsensitive);
            if (index>-1 && index<7) {
                line.remove('\n');
                line.remove('\r');
                return line.mid(index+1);
            }
        }
    }

    return QString();
}

static QString parseExt3Mu(const QByteArray &data)
{
    QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r)")), QString::SkipEmptyParts);

    foreach (QString line, lines) {
        if (line.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)) {
            line.remove('\n');
            line.remove('\r');
            return line;
        }
    }

    return QString();
}

static QString parseAsx(const QByteArray &data)
{
    QStringList lines=QString(data).split(QRegExp(QLatin1String("(\r\n|\n|\r|/>)")), QString::SkipEmptyParts);

    foreach (QString line, lines) {
        int ref=line.indexOf(QLatin1String(" href "), Qt::CaseInsensitive);
        int http=-1==ref ? -1 : line.indexOf(QLatin1String("http://"), Qt::CaseInsensitive);
        if (-1!=http) {
            QStringList parts=line.split('\"');
            foreach (QString part, parts) {
                if (part.startsWith(QLatin1String("http://"))) {
                    return part;
                }
            }
        }
    }

    return QString();
}

static QString parseXml(const QByteArray &data)
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
                    if (node.startsWith(QLatin1String("http://"))) {
                        return node;
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
    if (data.length()>10 && !strncasecmp(data.constData(), "[playlist]", 10)) {
        return parsePlaylist(data, QLatin1String("File"));
    } else if (data.length()>7 && (!strncasecmp(data.constData(), "#EXTM3U", 7) || !strncasecmp(data.constData(), "http://", 7))) {
        return parseExt3Mu(data);
    } else if (data.length()>5 && !strncasecmp(data.constData(), "<asx ", 5)) {
        return parseAsx(data);
    } else if (data.length()>11 && !strncasecmp(data.constData(), "[reference]", 11)) {
        return parsePlaylist(data, QLatin1String("Ref"));
    }
    else if (data.length()>5 && !strncasecmp(data.constData(), "<?xml", 5)) {
        QString rv;
        rv=parseXml(data);
    }

    return QString();
}

StreamFetcher::StreamFetcher(QObject *p)
    : QObject(p)
    , manager(0)
    , job(0)
{
}

StreamFetcher::~StreamFetcher()
{
}

void StreamFetcher::get(const QStringList &items, int insertRow)
{
    if (items.isEmpty()) {
        return;
    }

    todo=items;
    done.clear();
    row=insertRow;
    current=QString();
    if (!manager) {
        manager=new NetworkAccessManager(this);
        connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(jobFinished(QNetworkReply *)));
    }

    doNext();
}

void StreamFetcher::doNext()
{
    redirects=0;
    while (todo.count()) {
        current=todo.takeFirst();
        QUrl u(current);

        if (QLatin1String("http")==u.scheme()) {
            data.clear();
            job=manager->get(u);
            connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
            return;
        } else {
            done.append(current);
        }
    }

    if (todo.isEmpty() && !done.isEmpty()) {
        emit result(done, row);
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
        job->abort();
    }
    job=0;
}

void StreamFetcher::dataReady()
{
    data+=job->readAll();

    if (data.count()>constMaxData) {
        QNetworkReply *thisJob=job;
        disconnect(thisJob, SIGNAL(readyRead()), this, SLOT(dataReady()));
        jobFinished(thisJob);
        thisJob->abort();
    }
}

void StreamFetcher::jobFinished(QNetworkReply *reply)
{
    // We only handle 1 job at a time!
    if (reply==job) {
        if (!reply->error()) {
            QString u=parse(data);

            if (u.isEmpty() || u==current) {
                done.append(current);
            } else if (u.startsWith(QLatin1String("http://")) && ++redirects<constMaxRedirects) {
                // Redirect...
                current=u;
                data.clear();
                job=manager->get(u);
                connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
            } else {
                done.append(u);
            }
        } else {
            done.append(current);
        }

        doNext();
    }
}
