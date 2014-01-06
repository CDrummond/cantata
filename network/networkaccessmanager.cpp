/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "networkaccessmanager.h"
#include "networkproxyfactory.h"
#include "settings.h"
#include "config.h"
#include <QTimerEvent>
#include <QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(NetworkAccessManager, instance)
#endif

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void NetworkAccessManager::enableDebug()
{
    debugEnabled=true;
}

static const int constMaxRedirects=5;

NetworkJob::NetworkJob(NetworkAccessManager *p, const QUrl &u)
    : QObject(p)
    , numRedirects(0)
    , lastDownloadPc(0)
    , job(0)
    , origU(u)
{
    QTimer::singleShot(0, this, SLOT(jobFinished()));
}

NetworkJob::NetworkJob(QNetworkReply *j)
    : QObject(j->parent())
    , numRedirects(0)
    , lastDownloadPc(0)
    , job(j)
{
    origU=j->url();
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    connect(job, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
    connect(job, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
    connect(job, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
    connect(job, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProg(qint64, qint64)));
    connect(job, SIGNAL(destroyed(QObject *)), this, SLOT(jobDestroyed(QObject *)));
}

NetworkJob::~NetworkJob()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        disconnect(job, SIGNAL(destroyed(QObject *)), this, SLOT(jobDestroyed(QObject *)));
        job->abort();
        job->deleteLater();
        job=0;
    }
}

void NetworkJob::jobFinished()
{
    if (!job) {
        emit finished();
    }

    QNetworkReply *j=qobject_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }

    QVariant redirect = j->header(QNetworkRequest::LocationHeader);
    if (redirect.isValid() && ++numRedirects<constMaxRedirects) {
        job=static_cast<BASE_NETWORK_ACCESS_MANAGER *>(j->manager())->get(QNetworkRequest(redirect.toUrl()));
        DBUG << j->url().toString() << "redirected to" << job->url().toString();
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        j->deleteLater();
        return;
    }

    DBUG << job->url().toString() << job->error() << (0==job->error() ? QLatin1String("OK") : job->errorString());
    emit finished();
}

void NetworkJob::jobDestroyed(QObject *o)
{
    if (o==job) {
        job=0;
    }
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
    if (!instance) {
        instance=new NetworkAccessManager;
    }
    return instance;
    #endif
}

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : BASE_NETWORK_ACCESS_MANAGER(parent)
{
    enabled=Settings::self()->networkAccessEnabled();
    //#ifdef ENABLE_KDE_SUPPORT
    // TODO: Not sure if NetworkProxyFactory is required if building KDE support
    // with KIO::Integration::AccessManager. But, as we dont use that anyway...
    if (enabled) {
        NetworkProxyFactory::self();
    }
    //#endif
}

NetworkJob * NetworkAccessManager::get(const QNetworkRequest &req, int timeout)
{
    DBUG << req.url().toString() << enabled;
    if (!enabled) {
        return new NetworkJob(this, req.url());
    }

    #ifndef ENABLE_HTTPS_SUPPORT
    // Windows builds do not support HTTPS - unless QtNetwork is recompiled...
    NetworkJob *reply=0;
    if (QLatin1String("https")==req.url().scheme()) {
        QUrl httpUrl=req.url();
        httpUrl.setScheme(QLatin1String("http"));
        QNetworkRequest httpReq=req;
        httpReq.setUrl(httpUrl);
        reply = new NetworkJob(BASE_NETWORK_ACCESS_MANAGER::get(httpReq));
        reply->setOrigUrl(req.url());
    } else {
        reply = new NetworkJob(BASE_NETWORK_ACCESS_MANAGER::get(req));
    }
    #else
    NetworkJob *reply = new NetworkJob(BASE_NETWORK_ACCESS_MANAGER::get(req));
    #endif

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
