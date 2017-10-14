/*
 *Copyright (C) <2017>  Alex B
 *
 *This program is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 2 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef AVAHIDISCOVERY_H
#define AVAHIDISCOVERY_H

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <stdio.h>
#include <assert.h>

#include <QObject>


class AvahiDiscovery: public QObject
{
    Q_OBJECT

private:
    AvahiClient* m_client = nullptr;
    AvahiServiceBrowser* m_browser = nullptr;
    static AvahiDiscovery *ptr;


public:
    AvahiDiscovery();
    ~AvahiDiscovery();

    static void client_callback(
        AvahiClient *c,
        AvahiClientState state,
        AVAHI_GCC_UNUSED void * userdata);

    static void resolver_callback(
        AvahiServiceResolver *r,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiResolverEvent event,
        const char *name,
        const char *type,
        const char *domain,
        const char *host_name,
        const AvahiAddress *address,
        uint16_t port,
        AvahiStringList *txt,
        AvahiLookupResultFlags flags,
        void *userdata);

    static void browse_callback(
        AvahiServiceBrowser *b,
        AvahiIfIndex interface,
        AvahiProtocol protocol,
        AvahiBrowserEvent event,
        const char *name,
        const char *type,
        const char *domain,
        AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
        void* userdata);

signals:
    void mpdFound(QString name, QString address, int port);
    void mpdRemoved(QString name);

};


#endif // AVAHIDISCOVERY_H
