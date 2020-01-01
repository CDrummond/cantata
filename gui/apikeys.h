/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef API_KEYS_H
#define API_KEYS_H

#include <QMap>
#include <QString>
#include <QUrlQuery>
#include <QObject>
#include <time.h>

class QNetworkReply;

class ApiKeys : public QObject
{
    Q_OBJECT

public:
    static ApiKeys * self();

    enum Service {
        LastFm,
        FanArt,
        Dirble,
        ShoutCast,
        SoundCloud,

        NumServices
    };

    struct Details {
        Details(Service s, const QString &n, const QString &k, const QString &u)
            : srv(s), name(n), key(k), url(u) { }
        Service srv;
        QString name;
        QString key;
        QString url;
    };

    ApiKeys();

    void load();
    void save();
    QList<Details> getDetails();
    const QString &get(Service srv);
    void set(Service srv, const QString &key);
    void addKey(QUrlQuery &query, Service srv);
    QString addKey(const QString &url, Service srv);
    void setLimitReached(Service srv);
    bool isLimitReached(Service srv);
    bool isLimitReached(const QNetworkReply *job, Service srv);

Q_SIGNALS:
    void error(const QString &str);

private:
    QMap<QString, time_t> limitReached; // Set of keys where API limit has been reached
    QString defaultKeys[NumServices];
    QString userKeys[NumServices];
    QString queryItems[NumServices];
};

#endif
