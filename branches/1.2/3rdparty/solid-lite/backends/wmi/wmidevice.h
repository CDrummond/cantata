/*
    Copyright 2012 Patrick von Reth <vonreth@kde.org>
    Copyright 2005,2006 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_BACKENDS_WMI_WMIDEVICE_H
#define SOLID_BACKENDS_WMI_WMIDEVICE_H

#include <solid-lite/ifaces/device.h>
#include "wmiquery.h"

namespace Solid
{
namespace Backends
{
namespace Wmi
{
class WmiManager;
class WmiDevicePrivate;

struct ChangeDescription
{
    QString key;
    bool added;
    bool removed;
};

class WmiDevice : public Solid::Ifaces::Device
{
    Q_OBJECT

public:
    WmiDevice(const QString &udi);
    virtual ~WmiDevice();

    virtual QString udi() const;
    virtual QString parentUdi() const;

    virtual QString vendor() const;
    virtual QString product() const;
    virtual QString icon() const;
    virtual QStringList emblems() const;
    virtual QString description() const;

    virtual bool isValid() const;

    virtual QVariant property(const QString &key) const;

    virtual QMap<QString, QVariant> allProperties() const;

    virtual bool propertyExists(const QString &key) const;

    virtual bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const;
    virtual QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type);

    static QStringList generateUDIList(const Solid::DeviceInterface::Type &type);
    static bool exists(const QString &udi);
    const Solid::DeviceInterface::Type type() const;


    //TODO:rename the folowing methodes...
    static WmiQuery::Item win32LogicalDiskByDiskPartitionID(const QString &deviceID);
    static WmiQuery::Item win32DiskDriveByDiskPartitionID(const QString &deviceID);
    static WmiQuery::Item win32DiskPartitionByDeviceIndex(const QString &deviceID);
    static WmiQuery::Item win32DiskPartitionByDriveLetter(const QString &driveLetter);
    static WmiQuery::Item win32LogicalDiskByDriveLetter(const QString &driveLetter);


Q_SIGNALS:
    void propertyChanged(const QMap<QString,int> &changes);
    void conditionRaised(const QString &condition, const QString &reason);

private Q_SLOTS:
    void slotPropertyModified(int count, const QList<ChangeDescription> &changes);
    void slotCondition(const QString &condition, const QString &reason);

private:    
    WmiDevicePrivate *d;
    friend class WmiDevicePrivate;
};
}
}
}

#endif // SOLID_BACKENDS_WMI_WMIDEVICE_H
