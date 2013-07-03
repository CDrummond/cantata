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

#include "digitallyimported.h"
#include "settings.h"
#include "networkaccessmanager.h"
#include "qjson/parser.h"
#include "localize.h"
#include <QNetworkRequest>
#include <QTime>
#include <QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(DigitallyImported, instance)
#endif

static const char * constDiGroup="DigitallyImported";
static const QStringList constPremiumValues=QStringList() << QLatin1String("premium_high") << QLatin1String("premium_medium") << QLatin1String("premium");
static const QUrl constAuthUrl(QLatin1String("http://api.audioaddict.com/v1/di/members/authenticate"));
const QString DigitallyImported::constApiUserName=QLatin1String("ephemeron");
const QString DigitallyImported::constApiPassword=QLatin1String("dayeiph0ne@pp");
const QString DigitallyImported::constPublicValue=QLatin1String("public3");

DigitallyImported * DigitallyImported::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static DigitallyImported *instance=0;
    if(!instance) {
        instance=new DigitallyImported;
    }
    return instance;
    #endif
}

DigitallyImported::DigitallyImported()
    : job(0)
    , streamType(0)
    , timer(0)
{
    load();
}

DigitallyImported::~DigitallyImported()
{
    save();
}

void DigitallyImported::login()
{
    if (job) {
        job->deleteLater();
        job=0;
    }
    QNetworkRequest req(constAuthUrl);
    addAuthHeader(req);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    job=NetworkAccessManager::self()->post(req, "username="+QUrl::toPercentEncoding(userName)+"&password="+QUrl::toPercentEncoding(password));
    connect(job, SIGNAL(finished()), SLOT(loginResponse()));
}

void DigitallyImported::logout()
{
    if (job) {
        job->deleteLater();
        job=0;
    }
    listenHash=QString();
    expires=QDateTime();
    controlTimer();
}

void DigitallyImported::addAuthHeader(QNetworkRequest &req) const
{
    #if QT_VERSION < 0x050000
    req.setRawHeader("Authorization", "Basic "+QString("%1:%2").arg(constApiUserName, constApiPassword).toAscii().toBase64());
    #else
    req.setRawHeader("Authorization", "Basic "+QString("%1:%2").arg(constApiUserName, constApiPassword).toLatin1().toBase64());
    #endif
}

void DigitallyImported::load()
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constDiGroup);
    #else
    QSettings cfg;
    cfg.beginGroup(constDiGroup);
    #endif
    userName=GET_STRING("userName", userName);
    password=GET_STRING("password", password);
    listenHash=GET_STRING("listenHash", listenHash);
    streamType=GET_INT("streamType", streamType);
    QString ex=GET_STRING("expires", QString());

    status=i18n("Not logged in");
    if (ex.isEmpty()) {
        listenHash=QString();
    } else {
        expires=QDateTime::fromString(ex, Qt::ISODate);
        // If we have expired, or are about to expire in 5 minutes, then clear the hash...
        if (QDateTime::currentDateTime().secsTo(expires)<(5*60)) {
            listenHash=QString();
        } else if (!listenHash.isEmpty()) {
            status=i18n("Logged in");
        }
    }
    controlTimer();
}

void DigitallyImported::save()
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constDiGroup);
    #else
    QSettings cfg;
    cfg.beginGroup(constDiGroup);
    #endif

    SET_VALUE("userName", userName);
    SET_VALUE("password", password);
    SET_VALUE("listenHash", listenHash);
    SET_VALUE("streamType", streamType);
    SET_VALUE("expires", expires.toString(Qt::ISODate));

    #ifndef ENABLE_KDE_SUPPORT
    cfg.endGroup();
    #endif
    CFG_SYNC;
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
    job=0;

    status=listenHash=QString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (403==httpStatus) {
        status=reply->readAll();
        emit loginStatus(false, status);
        return;
    } else if (200!=httpStatus) {
        status=i18n("Unknown error");
        emit loginStatus(false, status);
        return;
    }

    QJson::Parser parser;
    QVariantMap data = parser.parse(reply).toMap();

    if (!data.contains("subscriptions")) {
        status=i18n("No subscriptions");
        emit loginStatus(false, status);
        return;
    }

    QVariantList subscriptions = data.value("subscriptions", QVariantList()).toList();
    if (subscriptions.isEmpty() || QLatin1String("active")!=subscriptions[0].toMap().value("status").toString()) {
        status=i18n("You do not have an active subscription");
        emit loginStatus(false, status);
        return;
    }

    if (!subscriptions[0].toMap().contains("expires_on") || !data.contains("listen_key")) {
        status=i18n("Unknown error");
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
    status=i18n("Logged in (expiry:%1)").arg(expires.toString(Qt::ISODate));
    controlTimer();
    emit loginStatus(true, status);
}

void DigitallyImported::timeout()
{
    listenHash=QString();
    emit loginStatus(false, i18n("Session expired"));
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
