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

#ifndef AVAHI_H
#define AVAHI_H

#include <QObject>
#include <QMap>

//class OrgFreedesktopAvahiDomainBrowserInterface;
class OrgFreedesktopAvahiServiceBrowserInterface;
class AvahiService;

class Avahi : public QObject
{
    Q_OBJECT
public:
    static QString domainToDNS(const QString &domain);

    static Avahi *self();

    Avahi();

    AvahiService * getService(const QString &name);

Q_SIGNALS:
    void serviceAdded(const QString &);
    void serviceRemoved(const QString &);

private Q_SLOTS:
    void addService(int, int, const QString &name, const QString &type, const QString &domain, uint);
    void removeService(int, int, const QString &name, const QString &type, const QString &domain, uint);

private:
    OrgFreedesktopAvahiServiceBrowserInterface *service;
    QMap<QString, AvahiService *> services;
};

#endif
