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

#include "networkproxyfactory.h"
#include <QMutexLocker>
#include <QSettings>
#include <QStringList>
#include <stdlib.h>

const char* NetworkProxyFactory::constSettingsGroup = "Proxy";

NetworkProxyFactory::NetworkProxyFactory()
    : mode(Mode_System)
    , type(QNetworkProxy::HttpProxy)
    , port(8080)
{
    #ifdef Q_OS_LINUX
    // Linux uses environment variables to pass proxy configuration information,
    // which systemProxyForQuery doesn't support for some reason.

    QStringList urls;
    urls << QString::fromLocal8Bit(getenv("HTTP_PROXY"));
    urls << QString::fromLocal8Bit(getenv("http_proxy"));
    urls << QString::fromLocal8Bit(getenv("ALL_PROXY"));
    urls << QString::fromLocal8Bit(getenv("all_proxy"));

    foreach (const QString& urlStr, urls) {
        if (urlStr.isEmpty()) {
            continue;
        }

        envUrl = QUrl(urlStr);
        break;
    }
    #endif

    reloadSettings();
}

NetworkProxyFactory * NetworkProxyFactory::self()
{
    static NetworkProxyFactory *instance=0;
    if (!instance) {
        instance = new NetworkProxyFactory;
    }

    return instance;
}

void NetworkProxyFactory::reloadSettings()
{
    QMutexLocker l(&mutex);

    QSettings s;
    s.beginGroup(constSettingsGroup);

    mode = Mode(s.value("mode", Mode_System).toInt());
    type = QNetworkProxy::ProxyType(s.value("type", QNetworkProxy::HttpProxy).toInt());
    hostname = s.value("hostname").toString();
    port = s.value("port", 8080).toInt();
    username = s.value("username").toString();
    password = s.value("password").toString();
}

QList<QNetworkProxy> NetworkProxyFactory::queryProxy(const QNetworkProxyQuery& query)
{
    QMutexLocker l(&mutex);
    QNetworkProxy ret;

    switch (mode) {
    case Mode_System:
        #ifdef Q_OS_LINUX
        Q_UNUSED(query);

        if (envUrl.isEmpty()) {
            ret.setType(QNetworkProxy::NoProxy);
        } else {
            ret.setHostName(envUrl.host());
            ret.setPort(envUrl.port());
            ret.setUser(envUrl.userName());
            ret.setPassword(envUrl.password());
            ret.setType(envUrl.scheme().startsWith("http") ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
        }
        break;
        #else
        return systemProxyForQuery(query);
        #endif
    case Mode_Direct:
        ret.setType(QNetworkProxy::NoProxy);
        break;
    case Mode_Manual:
        ret.setType(type);
        ret.setHostName(hostname);
        ret.setPort(port);
        if (!username.isEmpty()) {
            ret.setUser(username);
        }
        if (!password.isEmpty()) {
            ret.setPassword(password);
        }
        break;
    }

    return QList<QNetworkProxy>() << ret;
}
