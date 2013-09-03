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

#include "powermanagement.h"

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/Solid/PowerManagement>
#include <KDE/KGlobal>
#include <kdeversion.h>
K_GLOBAL_STATIC(PowerManagement, instance)
#else
#include "inhibitinterface.h"
#include "policyagentinterface.h"
#include "upowerinterface.h"
#endif

PowerManagement * PowerManagement::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static PowerManagement *instance=0;
    if(!instance) {
        instance=new PowerManagement;
    }
    return instance;
    #endif
}

PowerManagement::PowerManagement()
    : cookie(-1)
{
    #ifdef ENABLE_KDE_SUPPORT
    #if KDE_IS_VERSION(4, 7, 0)
    connect(Solid::PowerManagement::notifier(), SIGNAL(resumingFromSuspend()), this, SIGNAL(resuming()));
    #endif
    #else
    policy = new OrgKdeSolidPowerManagementPolicyAgentInterface(OrgKdeSolidPowerManagementPolicyAgentInterface::staticInterfaceName(),
                                                                QLatin1String("/org/kde/Solid/PowerManagement/PolicyAgent"),
                                                                QDBusConnection::sessionBus(), this);
    inhibit = new OrgFreedesktopPowerManagementInhibitInterface(OrgFreedesktopPowerManagementInhibitInterface::staticInterfaceName(),
                                                                QLatin1String("/org/freedesktop/PowerManagement/Inhibit"),
                                                                QDBusConnection::sessionBus(), this);
    upower = new OrgFreedesktopUPowerInterface(OrgFreedesktopUPowerInterface::staticInterfaceName(),
                                               QLatin1String("/org/freedesktop/UPower"), QDBusConnection::systemBus(), this);
    connect(upower, SIGNAL(Resuming()), this, SIGNAL(resuming()));
    #endif
}

void PowerManagement::beginSuppressingSleep(const QString &reason)
{
    if (-1!=cookie) {
        return;
    }
    #ifdef ENABLE_KDE_SUPPORT
    cookie=Solid::PowerManagement::beginSuppressingSleep(reason);
    #else
    QDBusReply<uint> reply;
    if (policy->isValid()) {
        reply = policy->AddInhibition((uint)1, QCoreApplication::applicationName(), reason);
    } else {
        // Fallback to the fd.o Inhibit interface
        reply = inhibit->Inhibit(QCoreApplication::applicationName(), reason);
    }
    cookie=reply.isValid() ? reply : -1;
    #endif
}

void PowerManagement::stopSuppressingSleep()
{
    if (-1==cookie) {
        return;
    }

    #ifdef ENABLE_KDE_SUPPORT
    Solid::PowerManagement::stopSuppressingSleep(cookie);
    #else
    if (policy->isValid()) {
        policy->ReleaseInhibition(cookie);
    } else {
        // Fallback to the fd.o Inhibit interface
        inhibit->UnInhibit(cookie);
    }
    #endif
    cookie=-1;
}
