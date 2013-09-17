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
    , job(j)
{
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
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

void NetworkJob::abort()
{
    if (job) {
        job->abort();
    }
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

QNetworkReply * NetworkAccessManager::get(const QNetworkRequest &req, int timeout)
{
    QNetworkReply *reply=BASE_NETWORK_ACCESS_MANAGER::get(req);

    if (0!=timeout) {
        connect(reply, SIGNAL(destroyed()), SLOT(replyFinished()));
        connect(reply, SIGNAL(finished()), SLOT(replyFinished()));
        timers[reply] = startTimer(timeout);
    }
    return reply;
}

NetworkJob * NetworkAccessManager::getNew(const QNetworkRequest &req, int timeout)
{
    NetworkJob *reply = new NetworkJob(BASE_NETWORK_ACCESS_MANAGER::get(req));

    if (0!=timeout) {
        connect(reply, SIGNAL(destroyed()), SLOT(replyFinished()));
        connect(reply, SIGNAL(finished()), SLOT(replyFinished()));
        newTimers[reply] = startTimer(timeout);
    }
    return reply;
}

void NetworkAccessManager::replyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (timers.contains(reply)) {
        killTimer(timers.take(reply));
    }

    NetworkJob *newReply = qobject_cast<NetworkJob*>(sender());
    if (newTimers.contains(newReply)) {
        killTimer(newTimers.take(newReply));
    }
}

void NetworkAccessManager::timerEvent(QTimerEvent *e)
{
    QNetworkReply *reply = timers.key(e->timerId());
    if (reply) {
        reply->abort();
    }

    NetworkJob *newReply = newTimers.key(e->timerId());
    if (newReply) {
        newReply->abort();
    }
}
