/*
    Copyright 2010 Rafael Fernández López <ereslibre@kde.org>

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

#ifndef SOLID_BACKENDS_UDEV_UDEVDEVICE_H
#define SOLID_BACKENDS_UDEV_UDEVDEVICE_H

#include "udev.h"

#include <solid-lite/ifaces/device.h>
#include <QtCore/QStringList>

namespace Solid
{
namespace Backends
{
namespace UDev
{

class UDevDevice : public Solid::Ifaces::Device
{
    Q_OBJECT

public:
    UDevDevice(const UdevQt::Device device);
    virtual ~UDevDevice();

    virtual QString udi() const;

    virtual QString parentUdi() const;

    virtual QString vendor() const;

    virtual QString product() const;

    virtual QString icon() const;

    virtual QStringList emblems() const;

    virtual QString description() const;

    virtual bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const;

    virtual QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type);

    QString device() const;

    QVariant property(const QString &key) const;
    QMap<QString, QVariant> allProperties() const;
    bool propertyExists(const QString &key) const;

    QString systemAttribute(const char *attribute) const;
    QString deviceName() const;
    QString devicePath() const;
    int deviceNumber() const;

    UdevQt::Device udevDevice();
private:
    UdevQt::Device m_device;
};

}
}
}
#endif // SOLID_BACKENDS_UDEV_UDEVDEVICE_H
