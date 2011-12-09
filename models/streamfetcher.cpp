/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtCore/QDebug>
#include <QtCore/QRegExp>
#ifdef XSPF
#include <xspf_c.h>
#endif

#ifdef SPIFF
#include <spiff/spiff_c.h>
#endif

static const int constMaxData = 12 * 1024;

static QString parsePlaylist(const QByteArray &data)
{
    QStringList lines=QString(data).split('\n', QString::SkipEmptyParts);

    foreach (QString line, lines) {
        if (line.startsWith("File", Qt::CaseInsensitive)) {
            int index=line.indexOf("=http://", Qt::CaseInsensitive);
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
    QStringList lines=QString(data).split(QRegExp("(\r\n|\n|\r)"), QString::SkipEmptyParts);

    foreach (QString line, lines) {
        if (line.startsWith("http://", Qt::CaseInsensitive)) {
            line.remove('\n');
            line.remove('\r');
            return line;
        }
    }

    return QString();
}

#ifdef XSPF
static QString parseXspf(const QByteArray &data)
{
    return QString();
}
#endif

#ifdef SPIFF
static QString parseSpiff(const QByteArray &data)
{
    return QString();
}
#endif

static QString parse(const QByteArray &data)
{
    if (data.length()>10 && !strncasecmp(data.constData(), "[playlist]", 10)) {
        return parsePlaylist(data);
    } else if (data.length()>7 && (!strncasecmp(data.constData(), "#EXTM3U", 7) || !strncasecmp(data.constData(), "http://", 7))) {
        return parseExt3Mu(data);
    }
#if defined XSPF || defined SPIFF
    else if (data.length()>5 && strncasecmp(data.constData(), "<?xml", 5)) {
        QString rv;
#ifdef XSPF
        rv=parseXspf(data);
#endif
#ifdef SPIFF
        if (rv.isEmpty()) {
            rv=parseSpiff(data);
        }
#endif
    }
#endif

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

void StreamFetcher::get(const QList<QUrl> &urls, int insertRow)
{
    if (urls.isEmpty()) {
        return;
    }

    todo=urls;
    done.clear();
    row=insertRow;
    pos=0;
    if (!manager) {
        manager=new NetworkAccessManager(this);
        connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(jobFinished(QNetworkReply *)));
    }

    job=manager->get(todo.at(pos));
    connect(job, SIGNAL(readyRead()), this, SLOT(dataReady()));
}

void StreamFetcher::cancel()
{
    todo.clear();
    done.clear();
    row=0;
    pos=0;
    data.clear();
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

            if (u.isEmpty()) {
                done.append(todo.at(pos).toString());
            } else {
                done.append(u);
            }
        }

        if (++pos==todo.count()) {
            if (done.count()) {
                emit result(done, row);
            }
        } else {
            // Next url!
            job=manager->get(todo.at(pos));
        }
    }
}
