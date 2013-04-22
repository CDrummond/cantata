/*
    Copyright 2009 Harald Fernengel <harry@kdevelop.org>

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

#ifndef SOLID_BACKENDS_IOKIT_IOKITDEVICE_H
#define SOLID_BACKENDS_IOKIT_IOKITDEVICE_H

#include <solid-lite/ifaces/device.h>
#include <IOKit/IOKitLib.h>

namespace Solid
{
namespace Backends
{
namespace IOKit
{
class IOKitDevicePrivate;
class IOKitManager;

class IOKitDevice : public Solid::Ifaces::Device
{
    Q_OBJECT

public:
    IOKitDevice(const QString &udi);
    virtual ~IOKitDevice();

    virtual QString udi() const;
    virtual QString parentUdi() const;

    virtual QString vendor() const;
    virtual QString product() const;
    virtual QString icon() const;
    virtual QStringList emblems() const;
    virtual QString description() const;

    virtual QVariant property(const QString &key) const;

    virtual QMap<QString, QVariant> allProperties() const;

    virtual bool propertyExists(const QString &key) const;

    virtual bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const;
    virtual QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type);

Q_SIGNALS:
    void propertyChanged(const QMap<QString,int> &changes);
    void conditionRaised(const QString &condition, const QString &reason);

private:
    friend class IOKitManager;
    IOKitDevice(const QString &udi, const io_registry_entry_t &entry);
    IOKitDevicePrivate * const d;
};
}
}
}

#endif // SOLID_BACKENDS_IOKIT_IOKITDEVICE_H
