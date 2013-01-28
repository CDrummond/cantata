/*
    Copyright 2005 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_IFACES_DEVICEMANAGER_H
#define SOLID_IFACES_DEVICEMANAGER_H

#include <QObject>

#include <QStringList>

#include <solid-lite/deviceinterface.h>

namespace Solid
{
namespace Ifaces
{
    /**
     * This class specifies the interface a backend will have to implement in
     * order to be used in the system.
     *
     * A device manager allows to query the underlying platform to discover the
     * available devices. It has also the responsibility to notify when a device
     * appears or disappears.
     */
    class DeviceManager : public QObject
    {
        Q_OBJECT

    public:
        /**
         * Constructs a DeviceManager
         */
        DeviceManager(QObject *parent = 0);
        /**
         * Destructs a DeviceManager object
         */
        virtual ~DeviceManager();

        /**
         * Retrieves the prefix used for the UDIs off all the devices
         * reported by the device manager
         */
        virtual QString udiPrefix() const = 0;

        /**
         * Retrieves a set of interfaces the backend supports
         */
        virtual QSet<Solid::DeviceInterface::Type> supportedInterfaces() const = 0;

        /**
         * Retrieves the Universal Device Identifier (UDI) of all the devices
         * available in the system. This identifier is unique for each device
         * in the system.
         *
         * @returns the UDIs of all the devices in the system
         */
        virtual QStringList allDevices() = 0;

        /**
         * Retrieves the Universal Device Identifier (UDI) of all the devices
         * matching the given constraints (parent and device interface)
         *
         * @param parentUdi UDI of the parent of the devices we're searching for, or QString()
         * if there's no constraint on the parent
         * @param type DeviceInterface type available on the devices we're looking for, or DeviceInterface::Unknown
         * if there's no constraint on the device interfaces
         * @returns the UDIs of all the devices having the given parent and device interface
         */
        virtual QStringList devicesFromQuery(const QString &parentUdi,
                                              Solid::DeviceInterface::Type type = Solid::DeviceInterface::Unknown) = 0;

        /**
         * Instantiates a new Device object from this backend given its UDI.
         *
         * @param udi the identifier of the device instantiated
         * @returns a new Device object if there's a device having the given UDI, 0 otherwise
         */
        virtual QObject *createDevice(const QString &udi) = 0;

    Q_SIGNALS:
        /**
         * This signal is emitted when a new device appears in the system.
         *
         * @param udi the new device identifier
         */
        void deviceAdded(const QString &udi);

        /**
         * This signal is emitted when a device disappears from the system.
         *
         * @param udi the old device identifier
         */
        void deviceRemoved(const QString &udi);
    };
}
}

#endif
