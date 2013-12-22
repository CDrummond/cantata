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

#ifndef SOLID_GENERICINTERFACE_H
#define SOLID_GENERICINTERFACE_H

#include <QMap>
#include <QVariant>

#include <solid-lite/solid_export.h>
#include <solid-lite/deviceinterface.h>

namespace Solid
{
    class GenericInterfacePrivate;
    class Device;

    /**
     * Generic interface to deal with a device. It exposes a set of properties
     * and is organized as a key/value set.
     *
     * Warning: Using this class could expose some backend specific details
     * and lead to non portable code. Use it at your own risk, or during
     * transitional phases when the provided device interfaces don't
     * provide the necessary methods.
     */
    class SOLID_EXPORT GenericInterface : public DeviceInterface
    {
        Q_OBJECT
        Q_ENUMS(PropertyChange)
        Q_DECLARE_PRIVATE(GenericInterface)
        friend class Device;

    public:
        /**
         * This enum type defines the type of change that can occur to a GenericInterface
         * property.
         *
         * - PropertyModified : A property value has changed in the device
         * - PropertyAdded : A new property has been added to the device
         * - PropertyRemoved : A property has been removed from the device
         */
        enum PropertyChange { PropertyModified, PropertyAdded, PropertyRemoved };

    private:
        /**
         * Creates a new GenericInterface object.
         * You generally won't need this. It's created when necessary using
         * Device::as().
         *
         * @param backendObject the device interface object provided by the backend
         * @see Solid::Device::as()
         */
        explicit GenericInterface(QObject *backendObject);

    public:
        /**
         * Destroys a Processor object.
         */
        virtual ~GenericInterface();


        /**
         * Get the Solid::DeviceInterface::Type of the GenericInterface device interface.
         *
         * @return the Processor device interface type
         * @see Solid::Ifaces::Enums::DeviceInterface::Type
         */
        static Type deviceInterfaceType() { return DeviceInterface::GenericInterface; }

        /**
         * Retrieves a property of the device.
         *
         * Warning: Using this method could expose some backend specific details
         * and lead to non portable code. Use it at your own risk, or during
         * transitional phases when the provided device interfaces don't
         * provide the necessary methods.
         *
         * @param key the property key
         * @return the actual value of the property, or QVariant() if the
         * property is unknown
         */
        QVariant property(const QString &key) const;

        /**
         * Retrieves a key/value map of all the known properties for the device.
         *
         * Warning: Using this method could expose some backend specific details
         * and lead to non portable code. Use it at your own risk, or during
         * transitional phases when the provided device interfaces don't
         * provide the necessary methods.
         *
         * @return all the properties of the device
         */
        QMap<QString, QVariant> allProperties() const;

        /**
         * Tests if a property exist in the device.
         *
         * Warning: Using this method could expose some backend specific details
         * and lead to non portable code. Use it at your own risk, or during
         * transitional phases when the provided device interfaces don't
         * provide the necessary methods.
         *
         * @param key the property key
         * @return true if the property is available in the device, false
         * otherwise
         */
        bool propertyExists(const QString &key) const;

    Q_SIGNALS:
        /**
         * This signal is emitted when a property is changed in the device.
         *
         * @param changes the map describing the property changes that
         * occurred in the device, keys are property name and values
         * describe the kind of change done on the device property
         * (added/removed/modified), it's one of the type Solid::Device::PropertyChange
         */
        void propertyChanged(const QMap<QString,int> &changes);

        /**
         * This signal is emitted when an event occurred in the device.
         * For example when a button is pressed.
         *
         * @param condition the condition name
         * @param reason a message explaining why the condition has been raised
         */
        void conditionRaised(const QString &condition, const QString &reason);
    };
}

#endif
