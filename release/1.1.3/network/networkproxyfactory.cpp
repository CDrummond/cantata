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


const char * NetworkProxyFactory::constSettingsGroup = "Proxy";

#if defined Q_OS_LINUX && QT_VERSION < 0x050000
// Taken from Qt5...
static bool ignoreProxyFor(const QNetworkProxyQuery &query)
{
    const QList<QByteArray> noProxyTokens = qgetenv("no_proxy").split(',');

    foreach (const QByteArray &rawToken, noProxyTokens) {
        QByteArray token = rawToken.trimmed();
        QString peerHostName = query.peerHostName();

        // Since we use suffix matching, "*" is our 'default' behaviour
        if (token.startsWith("*")) {
            token = token.mid(1);
        }

        // Harmonize trailing dot notation
        if (token.endsWith('.') && !peerHostName.endsWith('.')) {
            token = token.left(token.length()-1);
        }

        // We prepend a dot to both values, so that when we do a suffix match,
        // we don't match "donotmatch.com" with "match.com"
        if (!token.startsWith('.')) {
            token.prepend('.');
        }

        if (!peerHostName.startsWith('.')) {
            peerHostName.prepend('.');
        }

        if (peerHostName.endsWith(QString::fromLatin1(token))) {
            return true;
        }
    }

    return false;
}

// Taken from Qt5...
static QList<QNetworkProxy> systemProxyForQuery(const QNetworkProxyQuery &query)
{
    QList<QNetworkProxy> proxyList;

    if (ignoreProxyFor(query)) {
        return proxyList << QNetworkProxy::NoProxy;
    }

    // No need to care about casing here, QUrl lowercases values already
    const QString queryProtocol = query.protocolTag();
    QByteArray proxy_env;

    if (queryProtocol == QLatin1String("http")) {
        proxy_env = qgetenv("http_proxy");
    } else if (queryProtocol == QLatin1String("https")) {
        proxy_env = qgetenv("https_proxy");
    } else if (queryProtocol == QLatin1String("ftp")) {
        proxy_env = qgetenv("ftp_proxy");
    } else {
        proxy_env = qgetenv("all_proxy");
    }

    // Fallback to http_proxy is no protocol specific proxy was found
    if (proxy_env.isEmpty()) {
        proxy_env = qgetenv("http_proxy");
    }

    if (!proxy_env.isEmpty()) {
        QUrl url = QUrl(QString::fromLocal8Bit(proxy_env));
        if (url.scheme() == QLatin1String("socks5")) {
            QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, url.host(),
                    url.port() ? url.port() : 1080, url.userName(), url.password());
            proxyList << proxy;
        } else if (url.scheme() == QLatin1String("socks5h")) {
            QNetworkProxy proxy(QNetworkProxy::Socks5Proxy, url.host(),
                    url.port() ? url.port() : 1080, url.userName(), url.password());
            proxy.setCapabilities(QNetworkProxy::HostNameLookupCapability);
            proxyList << proxy;
        } else if ((url.scheme() == QLatin1String("http") || url.scheme().isEmpty())
                  && query.queryType() != QNetworkProxyQuery::UdpSocket
                  && query.queryType() != QNetworkProxyQuery::TcpServer) {
            QNetworkProxy proxy(QNetworkProxy::HttpProxy, url.host(),
                    url.port() ? url.port() : 8080, url.userName(), url.password());
            proxyList << proxy;
        }
    }
    if (proxyList.isEmpty()) {
        proxyList << QNetworkProxy::NoProxy;
    }

    return proxyList;
}
#endif

#ifdef ENABLE_PROXY_CONFIG
NetworkProxyFactory::NetworkProxyFactory()
    : mode(Mode_System)
    , type(QNetworkProxy::HttpProxy)
    , port(8080)
{
    reloadSettings();
}
#else
NetworkProxyFactory::NetworkProxyFactory()
{
}
#endif

NetworkProxyFactory * NetworkProxyFactory::self()
{
    static NetworkProxyFactory *instance=0;
    if (!instance) {
        instance = new NetworkProxyFactory;
    }

    return instance;
}

#ifdef ENABLE_PROXY_CONFIG
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
#endif

QList<QNetworkProxy> NetworkProxyFactory::queryProxy(const QNetworkProxyQuery& query)
{
    #ifdef ENABLE_PROXY_CONFIG
    QMutexLocker l(&mutex);
    QNetworkProxy ret;

    switch (mode) {
    case Mode_System:
        #if defined Q_OS_LINUX && QT_VERSION < 0x050000
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
    #elif defined Q_OS_LINUX && QT_VERSION < 0x050000
    return ::systemProxyForQuery(query);
    #else
    return QNetworkProxyFactory::systemProxyForQuery(query);
    #endif
}
