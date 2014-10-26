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

#ifndef SOLID_IFACE_OPTICALDRIVE_H
#define SOLID_IFACE_OPTICALDRIVE_H

#include <QtCore/QList>

#include <solid-lite/ifaces/storagedrive.h>
#include <solid-lite/opticaldrive.h>

namespace Solid
{
namespace Ifaces
{
    /**
     * This device interface is available on CD-ROM drives.
     *
     * A Cdrom is a storage that can handle optical discs.
     */
    class OpticalDrive : virtual public StorageDrive
    {
    public:
        /**
         * Destroys a Cdrom object.
         */
        virtual ~OpticalDrive();

        /**
         * Retrieves the medium types this drive supports.
         *
         * @return the flag set indicating the supported medium types
         */
        virtual Solid::OpticalDrive::MediumTypes supportedMedia() const = 0;

        /**
         * Retrieves the maximum read speed of this drive in kilobytes.
         *
         * @return the maximum read speed
         */
        virtual int readSpeed() const = 0;

        /**
         * Retrieves the maximum write speed of this drive in kilobytes.
         *
         * @return the maximum write speed
         */
        virtual int writeSpeed() const = 0;

        /**
         * Retrieves the list of supported write speeds of this drive in
         * kilobytes.
         *
         * @return the list of supported write speeds
         */
        virtual QList<int> writeSpeeds() const = 0;

        /**
         * Ejects any disc that could be contained in this drive.
         * If this drive is empty, but has a tray it'll be opened
         *
         * @return
         */
        virtual bool eject() = 0;

    protected:
    //Q_SIGNALS:
        /**
         * This signal is emitted when the eject button is pressed
         * on the drive.
         *
         * Please note that some (broken) drives doesn't report this event.
         * @param udi the UDI of the drive
         */
        virtual void ejectPressed(const QString &udi) = 0;

        virtual void ejectDone(Solid::ErrorType error, QVariant errorData, const QString &udi) = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::OpticalDrive, "org.kde.Solid.Ifaces.OpticalDrive/0.1")

#endif // SOLID_IFACE_OPTICALDRIVE_H
