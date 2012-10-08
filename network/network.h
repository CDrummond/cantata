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

#ifndef NETWORK_H
#define NETWORK_H

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QUrl>

class NetworkAccessManager;
class QNetworkReply;

class Network : public QObject
{
    Q_OBJECT

public:
    static Network * self();
    Network();
    void get(const QUrl &url, QObject *receiver, const char *slot, const QString &lang=QString());

private Q_SLOTS:
    void handleReply();
    void removeClient(QObject*);

private:
    NetworkAccessManager *manager;
    QList<const QObject*> clients;
    typedef struct {
        QObject *receiver;
        QString slot, lang;
    } RequestData;
    typedef QMap<QNetworkReply*, RequestData*> RequestMap;
    RequestMap requestData;
};

#endif // NETWORK_H
