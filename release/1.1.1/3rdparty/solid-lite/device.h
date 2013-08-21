/*
    Copyright 2005-2007 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_DEVICE_H
#define SOLID_DEVICE_H

#include <QVariant>

#include <QMap>
#include <QList>
#include <QSharedData>

#include <solid-lite/solid_export.h>

#include <solid-lite/deviceinterface.h>

namespace Solid
{
    class DevicePrivate;

    /**
     * This class allows applications to deal with devices available in the
     * underlying system.
     *
     * Device stores a reference to device data provided by the backend.
     * Device objects are designed to be used by value. Copying these objects
     * is quite cheap, so using pointers to the me is generally not needed.
     *
     * @author Kevin Ottens <ervin@kde.org>
     */
    class SOLID_EXPORT Device
    {
    public:
        /**
         * Retrieves all the devices available in the underlying system.
         *
         * @return the list of the devices available
         */
        static QList<Device> allDevices();

        /**
         * Retrieves a list of devices of the system given matching the given
         * constraints (parent and device interface type)
         *
         * @param type device interface type available on the devices we're looking for, or DeviceInterface::Unknown
         * if there's no constraint on the device interfaces
         * @param parentUdi UDI of the parent of the devices we're searching for, or QString()
         * if there's no constraint on the parent
         * @return the list of devices corresponding to the given constraints
         * @see Solid::Predicate
         */
        static QList<Device> listFromType(const DeviceInterface::Type &type,
                                          const QString &parentUdi = QString());

        /**
         * Retrieves a list of devices of the system given matching the given
         * constraints (parent and predicate)
         *
         * @param predicate Predicate that the devices we're searching for must verify
         * @param parentUdi UDI of the parent of the devices we're searching for, or QString()
         * if there's no constraint on the parent
         * @return the list of devices corresponding to the given constraints
         * @see Solid::Predicate
         */
        static QList<Device> listFromQuery(const Predicate &predicate,
                                           const QString &parentUdi = QString());

        /**
         * Convenience function see above.
         *
         * @param predicate
         * @param parentUdi
         * @return the list of devices
         */
        static QList<Device> listFromQuery(const QString &predicate,
                                           const QString &parentUdi = QString());


        /**
         * Constructs a device for a given Universal Device Identifier (UDI).
         *
         * @param udi the udi of the device to create
         */
        explicit Device(const QString &udi = QString());

        /**
         * Constructs a copy of a device.
         *
         * @param device the device to copy
         */
        Device(const Device &device);

        /**
         * Destroys the device.
         */
        ~Device();



        /**
         * Assigns a device to this device and returns a reference to it.
         *
         * @param device the device to assign
         * @return a reference to the device
         */
        Device &operator=(const Device &device);

        /**
         * Indicates if this device is valid.
         * A device is considered valid if it's available in the system.
         *
         * @return true if this device is available, false otherwise
         */
        bool isValid() const;


        /**
         * Retrieves the Universal Device Identifier (UDI).
         *
         * \warning Don't use the UDI for anything except communication with Solid. Also don't store
         * UDIs as there's no guarantee that the UDI stays the same when the hardware setup changed.
         * The UDI is a unique identifier that is local to the computer in question and for the
         * current boot session. The UDIs may change after a reboot.
         * Similar hardware in other computers may have different values; different
         * hardware could have the same UDI.
         *
         * @return the udi of the device
         */
        QString udi() const;

        /**
         * Retrieves the Universal Device Identifier (UDI)
         * of the Device's parent.
         *
         * @return the udi of the device's parent
         */
        QString parentUdi() const;


        /**
         * Retrieves the parent of the Device.
         *
         * @return the device's parent
         * @see parentUdi()
         */
        Device parent() const;



        /**
         * Retrieves the name of the device vendor.
         *
         * @return the vendor name
         */
        QString vendor() const;

        /**
         * Retrieves the name of the product corresponding to this device.
         *
         * @return the product name
         */
        QString product() const;

        /**
         * Retrieves the name of the icon representing this device.
         * The naming follows the freedesktop.org specification.
         *
         * @return the icon name
         */
        QString icon() const;

        /**
         * Retrieves the names of the emblems representing the state of this device.
         * The naming follows the freedesktop.org specification.
         *
         * @return the emblem names
         * @since 4.4
         */
        QStringList emblems() const;

        /**
         * Retrieves the description of device.
         *
         * @return the description
         * @since 4.4
         */
        QString description() const;

        /**
         * Tests if a device interface is available from the device.
         *
         * @param type the device interface type to query
         * @return true if the device interface is available, false otherwise
         */
        bool isDeviceInterface(const DeviceInterface::Type &type) const;

        /**
         * Retrieves a specialized interface to interact with the device corresponding to
         * a particular device interface.
         *
         * @param type the device interface type
         * @returns a pointer to the device interface interface if it exists, 0 otherwise
         */
        DeviceInterface *asDeviceInterface(const DeviceInterface::Type &type);

        /**
         * Retrieves a specialized interface to interact with the device corresponding to
         * a particular device interface.
         *
         * @param type the device interface type
         * @returns a pointer to the device interface interface if it exists, 0 otherwise
         */
        const DeviceInterface *asDeviceInterface(const DeviceInterface::Type &type) const;

        /**
         * Retrieves a specialized interface to interact with the device corresponding
         * to a given device interface.
         *
         * @returns a pointer to the device interface if it exists, 0 otherwise
         */
        template <class DevIface> DevIface *as()
        {
            DeviceInterface::Type type = DevIface::deviceInterfaceType();
            DeviceInterface *iface = asDeviceInterface(type);
            return qobject_cast<DevIface *>(iface);
        }

        /**
         * Retrieves a specialized interface to interact with the device corresponding
         * to a given device interface.
         *
         * @returns a pointer to the device interface if it exists, 0 otherwise
         */
        template <class DevIface> const DevIface *as() const
        {
            DeviceInterface::Type type = DevIface::deviceInterfaceType();
            const DeviceInterface *iface = asDeviceInterface(type);
            return qobject_cast<const DevIface *>(iface);
        }

        /**
         * Tests if a device provides a given device interface.
         *
         * @returns true if the interface is available, false otherwise
         */
        template <class DevIface> bool is() const
        {
            return isDeviceInterface(DevIface::deviceInterfaceType());
        }

    private:
        QExplicitlySharedDataPointer<DevicePrivate> d;
        friend class DeviceManagerPrivate;
    };
}

#endif
