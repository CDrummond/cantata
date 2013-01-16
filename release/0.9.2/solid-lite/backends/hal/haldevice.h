/*
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

#ifndef SOLID_BACKENDS_HAL_HALDEVICE_H
#define SOLID_BACKENDS_HAL_HALDEVICE_H

#include <solid-lite/ifaces/device.h>

class QDBusVariant;

namespace Solid
{
namespace Backends
{
namespace Hal
{
class HalManager;
class HalDevicePrivate;

struct ChangeDescription
{
    QString key;
    bool added;
    bool removed;
};

class HalDevice : public Solid::Ifaces::Device
{
    Q_OBJECT

public:
    HalDevice(const QString &udi);
    virtual ~HalDevice();

    virtual QString udi() const;
    virtual QString parentUdi() const;

    virtual QString vendor() const;
    virtual QString product() const;
    virtual QString icon() const;
    virtual QStringList emblems() const;
    virtual QString description() const;

    virtual bool queryDeviceInterface(const Solid::DeviceInterface::Type &type) const;
    virtual QObject *createDeviceInterface(const Solid::DeviceInterface::Type &type);

public:
    QVariant prop(const QString &key) const;
    QMap<QString, QVariant> allProperties() const;
    bool propertyExists(const QString &key) const;

Q_SIGNALS:
    void propertyChanged(const QMap<QString,int> &changes);
    void conditionRaised(const QString &condition, const QString &reason);

private Q_SLOTS:
    void slotPropertyModified(int count, const QList<ChangeDescription> &changes);
    void slotCondition(const QString &condition, const QString &reason);

private:
    QString storageDescription() const;
    QString volumeDescription() const;

    HalDevicePrivate *d;
};
}
}
}

#endif // SOLID_BACKENDS_HAL_HALDEVICE_H
