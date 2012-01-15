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

#include "networkproxyfactory.h"
#include <QtCore/QMutexLocker>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <stdlib.h>

NetworkProxyFactory* NetworkProxyFactory::sInstance = NULL;
const char* NetworkProxyFactory::kSettingsGroup = "Proxy";

NetworkProxyFactory::NetworkProxyFactory()
    : mode_(Mode_System)
    , type_(QNetworkProxy::HttpProxy)
    , port_(8080)
    , use_authentication_(false)
{
    #ifdef Q_OS_LINUX
    // Linux uses environment variables to pass proxy configuration information,
    // which systemProxyForQuery doesn't support for some reason.

    QStringList urls;
    urls << QString::fromLocal8Bit(getenv("HTTP_PROXY"));
    urls << QString::fromLocal8Bit(getenv("http_proxy"));
    urls << QString::fromLocal8Bit(getenv("ALL_PROXY"));
    urls << QString::fromLocal8Bit(getenv("all_proxy"));

    foreach (const QString& url_str, urls) {
        if (url_str.isEmpty()) {
            continue;
        }

        env_url_ = QUrl(url_str);
        break;
    }
    #endif

    ReloadSettings();
}

NetworkProxyFactory* NetworkProxyFactory::Instance()
{
    if (!sInstance) {
        sInstance = new NetworkProxyFactory;
    }

    return sInstance;
}

void NetworkProxyFactory::ReloadSettings()
{
    QMutexLocker l(&mutex_);

    QSettings s;
    s.beginGroup(kSettingsGroup);

    mode_ = Mode(s.value("mode", Mode_System).toInt());
    type_ = QNetworkProxy::ProxyType(s.value("type", QNetworkProxy::HttpProxy).toInt());
    hostname_ = s.value("hostname").toString();
    port_ = s.value("port", 8080).toInt();
    use_authentication_ = s.value("use_authentication", false).toBool();
    username_ = s.value("username").toString();
    password_ = s.value("password").toString();
}

QList<QNetworkProxy> NetworkProxyFactory::queryProxy(const QNetworkProxyQuery& query)
{
    QMutexLocker l(&mutex_);
    QNetworkProxy ret;

    switch (mode_) {
    case Mode_System:
        #ifdef Q_OS_LINUX
        Q_UNUSED(query);

        if (env_url_.isEmpty()) {
            ret.setType(QNetworkProxy::NoProxy);
        } else {
            ret.setHostName(env_url_.host());
            ret.setPort(env_url_.port());
            ret.setUser(env_url_.userName());
            ret.setPassword(env_url_.password());
            if (env_url_.scheme().startsWith("http")) {
                ret.setType(QNetworkProxy::HttpProxy);
            } else {
                ret.setType(QNetworkProxy::Socks5Proxy);
            }
        }
        break;
        #else
        return systemProxyForQuery(query);
        #endif
    case Mode_Direct:
        ret.setType(QNetworkProxy::NoProxy);
        break;
    case Mode_Manual:
        ret.setType(type_);
        ret.setHostName(hostname_);
        ret.setPort(port_);
        if (use_authentication_) {
            ret.setUser(username_);
            ret.setPassword(password_);
        }
        break;
    }

    return QList<QNetworkProxy>() << ret;
}
