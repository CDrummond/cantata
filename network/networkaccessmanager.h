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

#ifndef NETWORK_ACCESS_MANAGER_H
#define NETWORK_ACCESS_MANAGER_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIO/AccessManager>
#define BASE_NETWORK_ACCESS_MANAGER KIO::Integration::AccessManager
#else
#include <QNetworkAccessManager>
#define BASE_NETWORK_ACCESS_MANAGER QNetworkAccessManager
#endif
#include <QNetworkReply>
#include <QMap>

class QTimerEvent;

class NetworkAccessManager : public BASE_NETWORK_ACCESS_MANAGER
{
    Q_OBJECT

public:
    static NetworkAccessManager * self();

    NetworkAccessManager(QObject *parent=0);
    virtual ~NetworkAccessManager() { }

    QNetworkReply * get(const QNetworkRequest &req, int timeout=0);
    QNetworkReply * get(const QUrl &url, int timeout=0) { return get(QNetworkRequest(url), timeout); }

protected:
    void timerEvent(QTimerEvent *e);

private Q_SLOTS:
    void replyFinished();

private:
    QMap<QNetworkReply *, int> timers;
};

#endif // NETWORK_ACCESS_MANAGER_H
