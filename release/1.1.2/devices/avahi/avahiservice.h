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

#ifndef AVAHISERVICE_H
#define AVAHISERVICE_H

#include <QObject>

class OrgFreedesktopAvahiServiceResolverInterface;

class AvahiService : public QObject
{
    Q_OBJECT

public:
    AvahiService(const QString &n, const QString &type, const QString &domain);
    virtual ~AvahiService();

    const QString & getHost() const { return host; }
    ushort getPort() const { return port; }

private:
    void stop();

Q_SIGNALS:
    void serviceResolved(const QString &);

private Q_SLOTS:
    void resolved(int, int, const QString &name, const QString &, const QString &domain, const QString &h,
                  int, const QString &, ushort p, const QList<QByteArray> &txt, uint);
    void error(const QString &msg);

private:
    QString name;
    QString host;
    ushort port;
    OrgFreedesktopAvahiServiceResolverInterface *resolver;
};

#endif
