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

#ifndef SOLID_DEVICENOTIFIER_H
#define SOLID_DEVICENOTIFIER_H

#include <QtCore/QObject>
#include <QtCore/QList>

#include <solid-lite/solid_export.h>

namespace Solid
{
    /**
     * This class allow to query the underlying system to obtain information
     * about the hardware available.
     *
     * It's the unique entry point for hardware discovery. Applications should use
     * it to find devices, or to be notified about hardware changes.
     *
     * Note that it's implemented as a singleton and encapsulates the backend logic.
     *
     * @author Kevin Ottens <ervin@kde.org>
     */
    class SOLID_EXPORT DeviceNotifier : public QObject //krazy:exclude=dpointer (interface class)
    {
        Q_OBJECT
    public:
        static DeviceNotifier *instance();

    Q_SIGNALS:
        /**
         * This signal is emitted when a new device appear in the underlying system.
         *
         * @param udi the new device UDI
         */
        void deviceAdded(const QString &udi);

        /**
         * This signal is emitted when a device disappear from the underlying system.
         *
         * @param udi the old device UDI
         */
        void deviceRemoved(const QString &udi);
    };
}

#endif
