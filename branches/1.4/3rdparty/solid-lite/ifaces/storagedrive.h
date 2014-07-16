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

#ifndef SOLID_IFACES_STORAGEDRIVE_H
#define SOLID_IFACES_STORAGEDRIVE_H

#include <solid-lite/ifaces/block.h>
#include <solid-lite/storagedrive.h>

namespace Solid
{
namespace Ifaces
{
    /**
     * This device interface is available on storage devices.
     *
     * A storage is anything that can contain a set of volumes (card reader,
     * hard disk, cdrom drive...). It's a particular kind of block device.
     */
    class StorageDrive : virtual public Block
    {
    public:
        /**
         * Destroys a StorageDrive object.
         */
        virtual ~StorageDrive();


        /**
         * Retrieves the type of physical interface this storage device is
         * connected to.
         *
         * @return the bus type
         * @see Solid::StorageDrive::Bus
         */
        virtual Solid::StorageDrive::Bus bus() const = 0;

        /**
         * Retrieves the type of this storage drive.
         *
         * @return the drive type
         * @see Solid::StorageDrive::DriveType
         */
        virtual Solid::StorageDrive::DriveType driveType() const = 0;


        /**
         * Indicates if the media contained by this drive can be removed.
         *
         * For example memory card can be removed from the drive by the user,
         * while partitions can't be removed from hard disks.
         *
         * @return true if media can be removed, false otherwise.
         */
        virtual bool isRemovable() const = 0;

        /**
         * Indicates if this storage device can be plugged or unplugged while
         * the computer is running.
         *
         * @return true if this storage supports hotplug, false otherwise
         */
        virtual bool isHotpluggable() const = 0;

	/**
	* Retrieves this drives size in bytes.
	*
	* @return the size of this drive
	*/
	virtual qulonglong size() const = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::StorageDrive, "org.kde.Solid.Ifaces.StorageDrive/0.1")

#endif // SOLID_IFACES_STORAGEDRIVE_H
