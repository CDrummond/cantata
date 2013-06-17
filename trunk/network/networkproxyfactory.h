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

#ifndef NETWORKPROXYFACTORY_H
#define NETWORKPROXYFACTORY_H

#include <QMutex>
#include <QNetworkProxyFactory>
#include <QUrl>

class NetworkProxyFactory : public QNetworkProxyFactory
{
public:
      // These values are persisted
      enum Mode {
        Mode_System = 0,
        Mode_Direct = 1,
        Mode_Manual = 2
    };

    static NetworkProxyFactory * self();
    static const char * constSettingsGroup;

    // These methods are thread-safe
    void reloadSettings();
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& query);

private:
    NetworkProxyFactory();

private:
    QMutex mutex;
    Mode mode;
    QNetworkProxy::ProxyType type;
    QString hostname;
    int port;
    QString username;
    QString password;
    #ifdef Q_OS_LINUX
    QUrl envUrl;
    #endif
};

#endif // NETWORKPROXYFACTORY_H
