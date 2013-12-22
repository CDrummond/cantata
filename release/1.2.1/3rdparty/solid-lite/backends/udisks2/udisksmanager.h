/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010-2012 Lukáš Tinkl <ltinkl@redhat.com>

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

#ifndef UDISKS2MANAGER_H
#define UDISKS2MANAGER_H

#include "udisks2.h"
#include "udisksdevice.h"
#include "dbus/manager.h"

#include "solid-lite/ifaces/devicemanager.h"

#include <QDBusInterface>
#include <QSet>

namespace Solid
{
namespace Backends
{
namespace UDisks2
{

class Manager: public Solid::Ifaces::DeviceManager
{
    Q_OBJECT

public:
    Manager(QObject *parent);
    virtual QObject* createDevice(const QString& udi);
    virtual QStringList devicesFromQuery(const QString& parentUdi, Solid::DeviceInterface::Type type);
    virtual QStringList allDevices();
    virtual QSet< Solid::DeviceInterface::Type > supportedInterfaces() const;
    virtual QString udiPrefix() const;
    virtual ~Manager();

private Q_SLOTS:
    void slotInterfacesAdded(const QDBusObjectPath &object_path, const QVariantMapMap &interfaces_and_properties);
    void slotInterfacesRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void slotMediaChanged(const QDBusMessage &msg);

private:
    const QStringList &deviceCache();
    void introspect(const QString & path, bool checkOptical = false);
    void updateBackend(const QString & udi);
    QSet<Solid::DeviceInterface::Type> m_supportedInterfaces;
    org::freedesktop::DBus::ObjectManager m_manager;
    QStringList m_deviceCache;
};

}
}
}
#endif // UDISKS2MANAGER_H
