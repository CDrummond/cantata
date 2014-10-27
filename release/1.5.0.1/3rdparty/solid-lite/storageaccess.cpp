/*
    Copyright 2006-2007 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include "storageaccess.h"
#include "storageaccess_p.h"

#include "soliddefs_p.h"
#include <solid-lite/ifaces/storageaccess.h>

Solid::StorageAccess::StorageAccess(QObject *backendObject)
    : DeviceInterface(*new StorageAccessPrivate(), backendObject)
{
    connect(backendObject, SIGNAL(setupDone(Solid::ErrorType,QVariant,QString)),
            this, SIGNAL(setupDone(Solid::ErrorType,QVariant,QString)));
    connect(backendObject, SIGNAL(teardownDone(Solid::ErrorType,QVariant,QString)),
            this, SIGNAL(teardownDone(Solid::ErrorType,QVariant,QString)));
    connect(backendObject, SIGNAL(setupRequested(QString)),
            this, SIGNAL(setupRequested(QString)));
    connect(backendObject, SIGNAL(teardownRequested(QString)),
            this, SIGNAL(teardownRequested(QString)));

    connect(backendObject, SIGNAL(accessibilityChanged(bool,QString)),
            this, SIGNAL(accessibilityChanged(bool,QString)));
}

Solid::StorageAccess::StorageAccess(StorageAccessPrivate &dd, QObject *backendObject)
    : DeviceInterface(dd, backendObject)
{
    connect(backendObject, SIGNAL(setupDone(Solid::StorageAccess::SetupResult,QVariant,QString)),
            this, SIGNAL(setupDone(Solid::StorageAccess::SetupResult,QVariant,QString)));
    connect(backendObject, SIGNAL(teardownDone(Solid::StorageAccess::TeardownResult,QVariant,QString)),
            this, SIGNAL(teardownDone(Solid::StorageAccess::TeardownResult,QVariant,QString)));
    connect(backendObject, SIGNAL(setupRequested(QString)),
            this, SIGNAL(setupRequested(QString)));
    connect(backendObject, SIGNAL(teardownRequested(QString)),
            this, SIGNAL(teardownRequested(QString)));


    connect(backendObject, SIGNAL(accessibilityChanged(bool,QString)),
            this, SIGNAL(accessibilityChanged(bool,QString)));
}

Solid::StorageAccess::~StorageAccess()
{

}

bool Solid::StorageAccess::isAccessible() const
{
    Q_D(const StorageAccess);
    return_SOLID_CALL(Ifaces::StorageAccess *, d->backendObject(), false, isAccessible());
}

QString Solid::StorageAccess::filePath() const
{
    Q_D(const StorageAccess);
    return_SOLID_CALL(Ifaces::StorageAccess *, d->backendObject(), QString(), filePath());
}

bool Solid::StorageAccess::setup()
{
    Q_D(StorageAccess);
    return_SOLID_CALL(Ifaces::StorageAccess *, d->backendObject(), false, setup());
}

bool Solid::StorageAccess::teardown()
{
    Q_D(StorageAccess);
    return_SOLID_CALL(Ifaces::StorageAccess *, d->backendObject(), false, teardown());
}

bool Solid::StorageAccess::isIgnored() const
{
    Q_D(const StorageAccess);
    return_SOLID_CALL(Ifaces::StorageAccess *, d->backendObject(), true, isIgnored());
}

//#include "storageaccess.moc"
