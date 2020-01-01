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

#include "digitallyimported.h"
#include "support/configuration.h"
#include "network/networkaccessmanager.h"
#include "support/globalstatic.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QTime>
#include <QTimer>

static const char * constDiGroup="DigitallyImported";
static const QStringList constPremiumValues=QStringList() << QLatin1String("premium_high") << QLatin1String("premium_medium") << QLatin1String("premium");
static const QUrl constAuthUrl(QLatin1String("http://api.audioaddict.com/v1/di/members/authenticate"));
const QString DigitallyImported::constApiUserName=QLatin1String("ephemeron");
const QString DigitallyImported::constApiPassword=QLatin1String("dayeiph0ne@pp");
const QString DigitallyImported::constPublicValue=QLatin1String("public3");

GLOBAL_STATIC(DigitallyImported, instance)

DigitallyImported::DigitallyImported()
    : job(nullptr)
    , streamType(0)
    , timer(nullptr)
{
    load();
}

DigitallyImported::~DigitallyImported()
{
}

void DigitallyImported::login()
{
    if (job) {
        job->deleteLater();
        job=nullptr;
    }
    QNetworkRequest req(constAuthUrl);
    addAuthHeader(req);
    job=NetworkAccessManager::self()->postFormData(req, "username="+QUrl::toPercentEncoding(userName)+"&password="+QUrl::toPercentEncoding(password));
    connect(job, SIGNAL(finished()), SLOT(loginResponse()));
}

void DigitallyImported::logout()
{
    if (job) {
        job->deleteLater();
        job=nullptr;
    }
    listenHash=QString();
    expires=QDateTime();
    controlTimer();
}

void DigitallyImported::addAuthHeader(QNetworkRequest &req) const
{
    req.setRawHeader("Authorization", "Basic "+QString("%1:%2").arg(constApiUserName, constApiPassword).toLatin1().toBase64());
}

void DigitallyImported::load()
{
    Configuration cfg(constDiGroup);

    userName=cfg.get("userName", userName);
    password=cfg.get("password", password);
    listenHash=cfg.get("listenHash", listenHash);
    streamType=cfg.get("streamType", streamType);
    QString ex=cfg.get("expires", QString());

    status=tr("Not logged in");
    if (ex.isEmpty()) {
        listenHash=QString();
    } else {
        expires=QDateTime::fromString(ex, Qt::ISODate);
        // If we have expired, or are about to expire in 5 minutes, then clear the hash...
        if (QDateTime::currentDateTime().secsTo(expires)<(5*60)) {
            listenHash=QString();
        } else if (!listenHash.isEmpty()) {
            status=tr("Logged in");
        }
    }
    controlTimer();
}

void DigitallyImported::save()
{
    Configuration cfg(constDiGroup);

    cfg.set("userName", userName);
    cfg.set("password", password);
    cfg.set("listenHash", listenHash);
    cfg.set("streamType", streamType);
    cfg.set("expires", expires.toString(Qt::ISODate));
    emit updated();
}

bool DigitallyImported::isDiUrl(const QString &u) const
{
    if (!u.startsWith(QLatin1String("http://"))) {
        return false;
    }
    QUrl url(u);
    if (!url.host().startsWith(QLatin1String("listen."))) {
        return false;
    }
    QStringList pathParts=url.path().split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (2!=pathParts.count()) {
        return false;
    }
    return pathParts.at(0)==constPublicValue;
}

QString DigitallyImported::modifyUrl(const QString &u) const
{
    if (listenHash.isEmpty()) {
        return u;
    }
    QString premValue=constPremiumValues.at(streamType>0 && streamType<constPremiumValues.count() ? streamType : 0);
    QString url=u;
    return url.replace(constPublicValue, premValue)+QLatin1String("?hash=")+listenHash;
}

void DigitallyImported::loginResponse()
{
    QNetworkReply *reply=dynamic_cast<QNetworkReply *>(sender());

    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (reply!=job) {
        return;
    }
    job=nullptr;

    status=listenHash=QString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (403==httpStatus) {
        status=reply->readAll();
        emit loginStatus(false, status);
        return;
    } else if (200!=httpStatus) {
        status=tr("Unknown error");
        emit loginStatus(false, status);
        return;
    }

    QVariantMap data=QJsonDocument::fromJson(reply->readAll()).toVariant().toMap();

    if (!data.contains("subscriptions")) {
        status=tr("No subscriptions");
        emit loginStatus(false, status);
        return;
    }

    QVariantList subscriptions = data.value("subscriptions", QVariantList()).toList();
    if (subscriptions.isEmpty() || QLatin1String("active")!=subscriptions[0].toMap().value("status").toString()) {
        status=tr("You do not have an active subscription");
        emit loginStatus(false, status);
        return;
    }

    if (!subscriptions[0].toMap().contains("expires_on") || !data.contains("listen_key")) {
        status=tr("Unknown error");
        emit loginStatus(false, status);
        return;
    }

    QDateTime ex = QDateTime::fromString(subscriptions[0].toMap()["expires_on"].toString(), Qt::ISODate);
    QString lh = data["listen_key"].toString();

    if (ex!=expires || lh!=listenHash) {
        expires=ex;
        listenHash=lh;
        save();
    }
    status=tr("Logged in (expiry:%1)").arg(expires.toString(Qt::ISODate));
    controlTimer();
    emit loginStatus(true, status);
}

void DigitallyImported::timeout()
{
    listenHash=QString();
    emit loginStatus(false, tr("Session expired"));
}

void DigitallyImported::controlTimer()
{
    if (!expires.isValid() || QDateTime::currentDateTime().secsTo(expires)<15) {
        if (timer && timer->isActive()) {
            if (!listenHash.isEmpty()) {
                timeout();
            }
            timer->stop();
        }
    } else {
        if (!timer) {
            timer=new QTimer(this);
            connect(timer, SIGNAL(timeout()), SLOT(timeout()));
        }
        int secsTo=QDateTime::currentDateTime().secsTo(expires);

        if (secsTo>4) {
            timer->start((secsTo-3)*1000);
        } else {
            timeout();
        }
    }
}

#include "moc_digitallyimported.cpp"
