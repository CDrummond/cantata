/*
    Copyright 2006 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_IFACES_GENERICINTERFACE_H
#define SOLID_IFACES_GENERICINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QBool>
#include <QtCore/QMap>
#include <QtCore/QVariant>

namespace Solid
{
namespace Ifaces
{
    /**
     * Generic interface to deal with a device. It exposes a set of properties
     * and is organized a a key/value set.
     *
     * Warning: Using this class could expose some backend specific details
     * and lead to non portable code. Use it at your own risk, or during
     * transitional phases when the provided device interfaces don't
     * provide the necessary methods.
     */
    class GenericInterface
    {
    public:
        /**
         * Destroys a GenericInterface object.
         */
        virtual ~GenericInterface();

        /**
         * Retrieves the value of a property.
         *
         * @param key the property name
         * @returns the property value or QVariant() if the property doesn't exist
         */
        virtual QVariant property(const QString &key) const = 0;

        /**
         * Retrieves all the properties of this device.
         *
         * @returns all properties in a map
         */
        virtual QMap<QString, QVariant> allProperties() const = 0;

        /**
         * Tests if a property exist.
         *
         * @param key the property name
         * @returns true if the property exists in this device, false otherwise
         */
        virtual bool propertyExists(const QString &key) const = 0;

    protected:
    //Q_SIGNALS:
        /**
         * This signal is emitted when a property is changed in the device.
         *
         * @param changes the map describing the property changes that
         * occurred in the device, keys are property name and values
         * describe the kind of change done on the device property
         * (added/removed/modified), it's one of the type Solid::Device::PropertyChange
         */
        virtual void propertyChanged(const QMap<QString,int> &changes) = 0;

        /**
         * This signal is emitted when an event occurred in the device.
         * For example when a button is pressed.
         *
         * @param condition the condition name
         * @param reason a message explaining why the condition has been raised
         */
        virtual void conditionRaised(const QString &condition, const QString &reason) = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::GenericInterface, "org.kde.Solid.Ifaces.GenericInterface/0.1")

#endif
