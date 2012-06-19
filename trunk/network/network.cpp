/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* BE::MPC Qt4 client for MPD
 * Copyright (C) 2011 Thomas Luebking <thomas.luebking@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "network.h"
#include "networkaccessmanager.h"
#include "mpdparseutils.h"
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QDir>
#include <QtCore/qglobal.h>
#if defined Q_OS_WIN || defined CANTATA_ANDROID
#include <QtGui/QDesktopServices>
#endif
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
K_GLOBAL_STATIC(Network, instance)
#endif

QString Network::cacheDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN || defined CANTATA_ANDROID
    QString dir = QDesktopServices::storageLocation(QDesktopServices::CacheLocation)+"/";
    #else
    QString env = qgetenv("XDG_CACHE_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.cache" : env) + QLatin1String("/"PACKAGE_NAME"/");
    #endif
    if(!sub.isEmpty()) {
        dir+=sub;
    }
    dir=MPDParseUtils::fixPath(dir);
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? QDir::toNativeSeparators(dir) : QString();
}

Network * Network::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Network *instance=0;
    if(!instance) {
        instance=new Network;
    }
    return instance;
    #endif
}

Network::Network()
    : QObject(0)
{
    manager = new NetworkAccessManager(this);
}

void Network::get(const QUrl &url, QObject *receiver, const char *slot, const QString &lang)
{
    if (!receiver) {
        return; // haha
    }

    if (!clients.contains(receiver)) {
        clients << receiver;
        connect(receiver, SIGNAL(destroyed(QObject*)), self(), SLOT(removeClient(QObject*)));
    }

    QNetworkRequest request( url );
    // lie to prevent google etc. from believing we'd be some automated tool, abusing their ... errr ;-P
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux i686; rv:6.0) Gecko/20100101 Firefox/6.0");
    if (!lang.isEmpty()) {
        request.setRawHeader("Accept-Language", lang.toAscii() );
    }

    QNetworkReply *reply = manager->get(url);
    RequestMap::iterator data = requestData.insert(reply, new RequestData);
    (*data)->receiver = receiver;
    (*data)->slot = slot;
    (*data)->lang = lang;
    connect(reply, SIGNAL(finished()), self(), SLOT(handleReply()));
}

void Network::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    RequestMap::iterator data = requestData.find(reply);
    if (data == requestData.end()) {
        return;
    }

    if (!clients.contains((*data)->receiver)) {
        return;
    }

    QVariant redirect = reply->header( QNetworkRequest::LocationHeader );
    if ( redirect.isValid() ) {
        get( redirect.toUrl(), (*data)->receiver, (*data)->slot.toAscii().data(), (*data)->lang );
        delete *data;
        requestData.erase(data);
        reply->deleteLater();
        return;
    }

    reply->open(QIODevice::ReadOnly | QIODevice::Text);
    QString answer = QString::fromUtf8( reply->readAll() );
    reply->close();

    QMetaObject::invokeMethod((*data)->receiver, (*data)->slot.toAscii().data(), Qt::DirectConnection, Q_ARG(const QString &, answer));
    delete *data;
    requestData.erase(data);
    reply->deleteLater();
}

void Network::removeClient(QObject *c)
{
    clients.removeAll(c);
}
