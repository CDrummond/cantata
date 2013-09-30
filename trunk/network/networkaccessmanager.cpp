#include "networkaccessmanager.h"
#include <QTimerEvent>

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

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(NetworkAccessManager, instance)
#endif

static const int constMaxRedirects=5;

NetworkJob::NetworkJob(QNetworkReply *j)
    : QObject(j->parent())
    , numRedirects(0)
    , lastDownloadPc(0)
    , job(j)
{
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    connect(job, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
    connect(job, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
    connect(job, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
    connect(job, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProg(qint64, qint64)));
}

NetworkJob::~NetworkJob()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->abort();
        job->deleteLater();
    }
}

void NetworkJob::jobFinished()
{
    QNetworkReply *j=dynamic_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }

    QVariant redirect = j->header(QNetworkRequest::LocationHeader);
    if (redirect.isValid() && ++numRedirects<constMaxRedirects) {
        job=static_cast<BASE_NETWORK_ACCESS_MANAGER *>(j->manager())->get(QNetworkRequest(redirect.toUrl()));
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        j->deleteLater();
        return;
    }

    emit finished();
}

void NetworkJob::downloadProg(qint64 bytesReceived, qint64 bytesTotal)
{
    int pc=((bytesReceived*1.0)/(bytesTotal*1.0)*100.0)+0.5;
    pc=pc<0 ? 0 : (pc>100 ? 100 : pc);
    if (pc!=lastDownloadPc) {
        emit downloadPercent(pc);
    }
    emit downloadProgress(bytesReceived, bytesTotal);
}

void NetworkJob::handleReadyRead()
{
    QNetworkReply *j=dynamic_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }
    if (j->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
        return;
    }
    emit readyRead();
}

NetworkAccessManager * NetworkAccessManager::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static NetworkAccessManager *instance=0;
    if(!instance) {
        instance=new NetworkAccessManager;
    }
    return instance;
    #endif
}

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : BASE_NETWORK_ACCESS_MANAGER(parent)
{
}

NetworkJob * NetworkAccessManager::get(const QNetworkRequest &req, int timeout)
{
    NetworkJob *reply = new NetworkJob(BASE_NETWORK_ACCESS_MANAGER::get(req));

    if (0!=timeout) {
        connect(reply, SIGNAL(destroyed()), SLOT(replyFinished()));
        connect(reply, SIGNAL(finished()), SLOT(replyFinished()));
        timers[reply] = startTimer(timeout);
    }
    return reply;
}

void NetworkAccessManager::replyFinished()
{
    NetworkJob *job = qobject_cast<NetworkJob*>(sender());
    if (timers.contains(job)) {
        killTimer(timers.take(job));
    }
}

void NetworkAccessManager::timerEvent(QTimerEvent *e)
{
    NetworkJob *job = timers.key(e->timerId());
    if (job) {
        job->abort();
    }
}
