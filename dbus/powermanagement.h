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

#ifndef POWERMANAGEMENT_H
#define POWERMANAGEMENT_H

#include <QObject>
#include <QString>

#ifndef ENABLE_KDE_SUPPORT
class OrgKdeSolidPowerManagementPolicyAgentInterface;
class OrgFreedesktopPowerManagementInhibitInterface;
class OrgFreedesktopUPowerInterface;
#endif
    
class PowerManagement : public QObject
{
    Q_OBJECT

public:
    static PowerManagement * self();
    PowerManagement();

    void beginSuppressingSleep();
    void stopSuppressingSleep();
     
Q_SIGNALS:
    void resuming();

private:
    int cookie;
    #ifndef ENABLE_KDE_SUPPORT
    OrgKdeSolidPowerManagementPolicyAgentInterface *policy;
    OrgFreedesktopPowerManagementInhibitInterface *inhibit;
    OrgFreedesktopUPowerInterface *upower;
    #endif
};

#endif
