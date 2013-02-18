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
#include "servicebrowserinterface.h"
#include <QtDBus>
#include <QUrl>
#include <QDebug>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(Avahi, instance)
#endif

static bool isLocalDomain(const QString &d)
{
    return QLatin1String("local")==d.section('.', -1, -1).toLower();
}

QString Avahi::domainToDNS(const QString &domain)
{
    return isLocalDomain(domain) ? domain : QUrl::toAce(domain);
}

static const QLatin1String constServiceType("_smb._tcp");

Avahi * Avahi::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Avahi *instance=0;
    if(!instance) {
        instance=new Avahi;
    }
    return instance;
    #endif
}

Avahi::Avahi()
{
    org::freedesktop::Avahi::Server server("org.freedesktop.Avahi", "/", QDBusConnection::systemBus());
    QDBusReply<QDBusObjectPath> reply=server.ServiceBrowserNew(-1, -1, constServiceType, domainToDNS(QString()), 0);

    if (reply.isValid()) {
        service=new OrgFreedesktopAvahiServiceBrowserInterface("org.freedesktop.Avahi", reply.value().path(), QDBusConnection::systemBus());
        connect(service, SIGNAL(ItemNew(int,int,QString,QString,QString,uint)), SLOT(addService(int,int,QString,QString,QString,uint)));
        connect(service, SIGNAL(ItemRemove(int,int,QString,QString,QString,uint)), SLOT(removeService(int,int,QString,QString,QString,uint)));
    }
}

AvahiService * Avahi::getService(const QString &name)
{
    return services.contains(name) ? services[name] : 0;
}

void Avahi::addService(int, int, const QString &name, const QString &type, const QString &domain, uint)
{
    if (isLocalDomain(domain) && !services.contains(name)) {
        services.insert(name, new AvahiService(name, type, domain));
    }
}

void Avahi::removeService(int, int, const QString &name, const QString &, const QString &domain, uint)
{
    if (isLocalDomain(domain) && services.contains(name)) {
        services[name]->deleteLater();
        services.remove(name);
    }
}
