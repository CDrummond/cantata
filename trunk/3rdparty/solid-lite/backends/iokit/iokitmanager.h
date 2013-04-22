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

#ifndef SOLID_BACKENDS_IOKIT_IOKITMANAGER_H
#define SOLID_BACKENDS_IOKIT_IOKITMANAGER_H

#include <solid-lite/ifaces/devicemanager.h>
#include <solid-lite/deviceinterface.h>

#include <QVariant>
#include <QStringList>

namespace Solid
{
namespace Backends
{
namespace IOKit
{
class IOKitManagerPrivate;

class IOKitManager : public Solid::Ifaces::DeviceManager
{
    Q_OBJECT

public:
    IOKitManager(QObject *parent);
    virtual ~IOKitManager();

    virtual QString udiPrefix() const ;
    virtual QSet<Solid::DeviceInterface::Type> supportedInterfaces() const;

    virtual QStringList allDevices();
    virtual QStringList devicesFromQuery(const QString &parentUdi,
                                         Solid::DeviceInterface::Type type);
    virtual QObject *createDevice(const QString &udi);

private:
    IOKitManagerPrivate *d;
};
}
}
}

#endif // SOLID_BACKENDS_IOKIT_IOKITMANAGER_H

