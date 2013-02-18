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

#include "avahi.h"
#include "avahiservice.h"
#include "serverinterface.h"
#include "serviceresolverinterface.h"
Q_DECLARE_METATYPE(QList<QByteArray>)

AvahiService::AvahiService(const QString &name, const QString &type, const QString &domain)
    : port(0)
{
    static bool registeredTypes=false;
    if (!registeredTypes) {
        qDBusRegisterMetaType<QList<QByteArray> >();
        registeredTypes=true;
    }

    org::freedesktop::Avahi::Server server("org.freedesktop.Avahi", "/", QDBusConnection::systemBus());
    QDBusReply<QDBusObjectPath> reply=server.ServiceResolverNew(-1, -1, name, type, Avahi::domainToDNS(domain), -1, 8 /*AVAHI_LOOKUP_NO_ADDRESS|AVAHI_LOOKUP_NO_TXT*/);

    if (reply.isValid()) {
        resolver=new OrgFreedesktopAvahiServiceResolverInterface("org.freedesktop.Avahi", reply.value().path(), QDBusConnection::systemBus());
        connect(resolver, SIGNAL(Found(int,int,const QString &,const QString &,const QString &,const QString &, int, const QString &,ushort,const QList<QByteArray>&, uint)),
                this, SLOT(resolved(int,int,const QString &,const QString &,const QString &,const QString &, int, const QString &,ushort, const QList<QByteArray>&, uint)));
        connect(resolver, SIGNAL(Failure(QString)), this, SLOT(error(QString)));
    }
}

AvahiService::~AvahiService()
{
    stop();
}

void AvahiService::resolved(int, int, const QString &name, const QString &, const QString &, const QString &h, int, const QString &, ushort p, const QList<QByteArray> &, uint)
{
    Q_UNUSED(name)
    port=p;
    host=h;
    stop();
}

void AvahiService::error(const QString &)
{
    stop();
}

void AvahiService::stop()
{
    if (resolver) {
        resolver->Free();
        resolver->deleteLater();
        disconnect(resolver, SIGNAL(Found(int,int,const QString &,const QString &,const QString &,const QString &, int, const QString &,ushort,const QList<QByteArray>&, uint)),
                   this, SLOT(resolved(int,int,const QString &,const QString &,const QString &,const QString &, int, const QString &,ushort, const QList<QByteArray>&, uint)));
        disconnect(resolver, SIGNAL(Failure(QString)), this, SLOT(error(QString)));
        resolver=0;
    }
}
