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

#include "networkaccessmanager.h"
#include "networkproxyfactory.h"
#include "gui/settings.h"
#include "config.h"
#include "support/globalstatic.h"
#include <QTimerEvent>
#include <QTimer>
#include <QSslSocket>
#include <QCoreApplication>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void NetworkAccessManager::enableDebug()
{
    debugEnabled=true;
}

static bool networkAccessEnabled=true;
void NetworkAccessManager::disableNetworkAccess()
{
    networkAccessEnabled = false;
}

static const int constMaxRedirects=5;

NetworkJob::NetworkJob(NetworkAccessManager *p, const QUrl &u)
    : QObject(p)
    , numRedirects(0)
    , lastDownloadPc(0)
    , job(nullptr)
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
    connectJob();
    DBUG << (void *)this << (void *)job;
}

NetworkJob::~NetworkJob()
{
    DBUG << (void *)this << (void *)job;
    cancelJob();
}

void NetworkJob::cancelAndDelete()
{
    DBUG << (void *)this << (void *)job;
    cancelJob();
    deleteLater();
}

void NetworkJob::connectJob()
{
    if (!job) {
        return;
    }

    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    connect(job, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
    connect(job, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
    connect(job, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
    connect(job, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProg(qint64, qint64)));
    connect(job, SIGNAL(destroyed(QObject *)), this, SLOT(jobDestroyed(QObject *)));
}

void NetworkJob::cancelJob()
{
    DBUG << (void *)this << (void *)job;
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        disconnect(job, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
        disconnect(job, SIGNAL(error(QNetworkReply::NetworkError)), this, SIGNAL(error(QNetworkReply::NetworkError)));
        disconnect(job, SIGNAL(uploadProgress(qint64, qint64)), this, SIGNAL(uploadProgress(qint64, qint64)));
        disconnect(job, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProg(qint64, qint64)));
        disconnect(job, SIGNAL(destroyed(QObject *)), this, SLOT(jobDestroyed(QObject *)));
        job->abort();
        job->close();
        job->deleteLater();
        job=nullptr;
    }
}

void NetworkJob::abortJob()
{
    DBUG << (void *)this << (void *)job;
    if (job) {
        job->abort();
    }
}

void NetworkJob::jobFinished()
{
    DBUG << (void *)this << (void *)job;
    if (!job) {
        emit finished();
    }

    QNetworkReply *j=qobject_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }

    QVariant redirect = j->header(QNetworkRequest::LocationHeader);
    if (redirect.isValid() && ++numRedirects<constMaxRedirects) {
        QNetworkRequest newReq(redirect.toUrl());
        QNetworkRequest origReq(job->request());
        // Copy headers...
        const QList<QByteArray> &headers=origReq.rawHeaderList();;
        for (const QByteArray &header: headers) {
            newReq.setRawHeader(header, origReq.rawHeader(header));
        }
        QNetworkReply *newJob=static_cast<QNetworkAccessManager *>(j->manager())->get(newReq);
        DBUG << j->url().toString() << "redirected to" << newJob->url().toString();

        cancelJob();
        job=newJob;
        connectJob();
        return;
    }

    DBUG << job->url().toString() << job->error() << (0==job->error() ? QLatin1String("OK") : job->errorString());
    emit finished();
}

void NetworkJob::jobDestroyed(QObject *o)
{
    DBUG << (void *)this << (void *)job;
    if (o==job) {
        job=nullptr;
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
    DBUG << (void *)this << (void *)job;
    QNetworkReply *j=dynamic_cast<QNetworkReply *>(sender());
    if (!j || j!=job) {
        return;
    }
    if (j->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
        return;
    }
    emit readyRead();
}

GLOBAL_STATIC(NetworkAccessManager, instance)

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{
    if (networkAccessEnabled) {
        NetworkProxyFactory::self();
    }
}

NetworkJob * NetworkAccessManager::get(const QNetworkRequest &req, int timeout)
{
    DBUG << req.url().toString() << networkAccessEnabled;
    if (!networkAccessEnabled) {
        return new NetworkJob(this, req.url());
    }

    // Set User-Agent - required for some Podcasts...
    QByteArray userAgent = QString("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()).toUtf8();
    if (req.hasRawHeader("User-Agent")) {
        userAgent+=' '+req.rawHeader("User-Agent");
    }
    QNetworkRequest request=req;
    request.setRawHeader("User-Agent", userAgent);

    // Windows builds do not support HTTPS - unless QtNetwork is recompiled...
    NetworkJob *reply=nullptr;
    bool supportsSsl = false;
    #ifndef QT_NO_SSL
    supportsSsl = QSslSocket::supportsSsl();
    #endif
    if (QLatin1String("https")==req.url().scheme() && !supportsSsl) {
        QUrl httpUrl=request.url();
        httpUrl.setScheme(QLatin1String("http"));
        request.setUrl(httpUrl);
        DBUG << "no ssl, use" << httpUrl.toString();
        reply = new NetworkJob(QNetworkAccessManager::get(request));
        reply->setOrigUrl(req.url());
    } else {
        reply = new NetworkJob(QNetworkAccessManager::get(request));
    }

    if (0!=timeout) {
        connect(reply, SIGNAL(destroyed()), SLOT(replyFinished()));
        connect(reply, SIGNAL(finished()), SLOT(replyFinished()));
        timers[reply] = startTimer(timeout);
    }
    return reply;
}

struct FakeNetworkReply : public QNetworkReply
{
    FakeNetworkReply() : QNetworkReply(nullptr)
    {
        setError(QNetworkReply::ConnectionRefusedError, QString());
        QTimer::singleShot(0, this, SIGNAL(finished()));
    }
    void abort() override { }
    qint64 readData(char *, qint64) override { return 0; }
    qint64 writeData(const char *, qint64) override { return 0; }
};

QNetworkReply * NetworkAccessManager::postFormData(QNetworkRequest req, const QByteArray &data)
{
    DBUG << req.url().toString() << networkAccessEnabled << data.length();
    if (networkAccessEnabled) {
        if (!data.isEmpty()) {
            req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
        }
        return QNetworkAccessManager::post(req, data);
    } else {
        return new FakeNetworkReply();
    }
}

void NetworkAccessManager::replyFinished()
{
    NetworkJob *job = static_cast<NetworkJob*>(sender());
    DBUG << (void *)job;
    if (timers.contains(job)) {
        killTimer(timers.take(job));
    }
}

void NetworkAccessManager::timerEvent(QTimerEvent *e)
{
    NetworkJob *job = timers.key(e->timerId());
    DBUG << (void *)job;
    if (job) {
        job->abortJob();
    }
}

#include "moc_networkaccessmanager.cpp"
