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

#include "storagedrive.h"
#include "storagedrive_p.h"

#include "soliddefs_p.h"
#include <solid-lite/ifaces/storagedrive.h>
#include "predicate.h"
#include "storageaccess.h"
#include "device.h"
#include "device_p.h"

Solid::StorageDrive::StorageDrive(QObject *backendObject)
    : DeviceInterface(*new StorageDrivePrivate(), backendObject)
{
}

Solid::StorageDrive::StorageDrive(StorageDrivePrivate &dd, QObject *backendObject)
    : DeviceInterface(dd, backendObject)
{

}

Solid::StorageDrive::~StorageDrive()
{

}

Solid::StorageDrive::Bus Solid::StorageDrive::bus() const
{
    Q_D(const StorageDrive);
    return_SOLID_CALL(Ifaces::StorageDrive *, d->backendObject(), Platform, bus());
}

Solid::StorageDrive::DriveType Solid::StorageDrive::driveType() const
{
    Q_D(const StorageDrive);
    return_SOLID_CALL(Ifaces::StorageDrive *, d->backendObject(), HardDisk, driveType());
}

bool Solid::StorageDrive::isRemovable() const
{
    Q_D(const StorageDrive);
    return_SOLID_CALL(Ifaces::StorageDrive *, d->backendObject(), false, isRemovable());
}

bool Solid::StorageDrive::isHotpluggable() const
{
    Q_D(const StorageDrive);
    return_SOLID_CALL(Ifaces::StorageDrive *, d->backendObject(), false, isHotpluggable());
}

qulonglong Solid::StorageDrive::size() const
{
    Q_D(const StorageDrive);
    return_SOLID_CALL(Ifaces::StorageDrive *, d->backendObject(), false, size());
}

bool Solid::StorageDrive::isInUse() const
{
    Q_D(const StorageDrive);
    Predicate p(DeviceInterface::StorageAccess);
    QList<Device> devices = Device::listFromQuery(p, d->devicePrivate()->udi());

    bool inUse = false;
    foreach (const Device &dev, devices)	{
        if (dev.is<Solid::StorageAccess>()) {
            const Solid::StorageAccess* access = dev.as<Solid::StorageAccess>();
            inUse |= (access->isAccessible());
        }
    }
    return inUse;
}

//#include "storagedrive.moc"

