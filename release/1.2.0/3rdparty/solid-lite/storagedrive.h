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

#ifndef SOLID_STORAGEDRIVE_H
#define SOLID_STORAGEDRIVE_H

#include <solid-lite/solid_export.h>

#include <solid-lite/deviceinterface.h>

namespace Solid
{
    class StorageDrivePrivate;
    class Device;

    /**
     * This device interface is available on storage devices.
     *
     * A storage is anything that can contain a set of volumes (card reader,
     * hard disk, cdrom drive...). It's a particular kind of block device.
     */
    class SOLID_EXPORT StorageDrive : public DeviceInterface
    {
        Q_OBJECT
        Q_ENUMS(Bus DriveType)
        Q_PROPERTY(Bus bus READ bus)
        Q_PROPERTY(DriveType driveType READ driveType)
        Q_PROPERTY(bool removable READ isRemovable)
        Q_PROPERTY(bool hotpluggable READ isHotpluggable)
        Q_PROPERTY(bool inUse READ isInUse)
        Q_PROPERTY(qulonglong size READ size)
        Q_DECLARE_PRIVATE(StorageDrive)
        friend class Device;

    public:
        /**
         * This enum type defines the type of bus a storage device is attached to.
         *
         * - Ide : An Integrated Drive Electronics (IDE) bus, also known as ATA
         * - Usb : An Universal Serial Bus (USB)
         * - Ieee1394 : An Ieee1394 bus, also known as Firewire
         * - Scsi : A Small Computer System Interface bus
         * - Sata : A Serial Advanced Technology Attachment (SATA) bus
         * - Platform : A legacy bus that is part of the underlying platform
         */
        enum Bus { Ide, Usb, Ieee1394, Scsi, Sata, Platform };

        /**
         * This enum type defines the type of drive a storage device can be.
         *
         * - HardDisk : A hard disk
         * - CdromDrive : An optical drive
         * - Floppy : A floppy disk drive
         * - Tape : A tape drive
         * - CompactFlash : A Compact Flash card reader
         * - MemoryStick : A Memory Stick card reader
         * - SmartMedia : A Smart Media card reader
         * - SdMmc : A SecureDigital/MultiMediaCard card reader
         * - Xd : A xD card reader
         */
        enum DriveType { HardDisk, CdromDrive, Floppy, Tape, CompactFlash, MemoryStick, SmartMedia, SdMmc, Xd };


    private:
        /**
         * Creates a new StorageDrive object.
         * You generally won't need this. It's created when necessary using
         * Device::as().
         *
         * @param backendObject the device interface object provided by the backend
         * @see Solid::Device::as()
         */
        explicit StorageDrive(QObject *backendObject);

    public:
        /**
         * Destroys a StorageDrive object.
         */
        virtual ~StorageDrive();


        /**
         * Get the Solid::DeviceInterface::Type of the StorageDrive device interface.
         *
         * @return the StorageDrive device interface type
         * @see Solid::DeviceInterface::Type
         */
        static Type deviceInterfaceType() { return DeviceInterface::StorageDrive; }


        /**
         * Retrieves the type of physical interface this storage device is
         * connected to.
         *
         * @return the bus type
         * @see Solid::StorageDrive::Bus
         */
        Bus bus() const;

        /**
         * Retrieves the type of this storage drive.
         *
         * @return the drive type
         * @see Solid::StorageDrive::DriveType
         */
        DriveType driveType() const;

        /**
         * Indicates if the media contained by this drive can be removed.
         *
         * For example memory card can be removed from the drive by the user,
         * while partitions can't be removed from hard disks.
         *
         * @return true if media can be removed, false otherwise.
         */
        bool isRemovable() const;

        /**
         * Indicates if this storage device can be plugged or unplugged while
         * the computer is running.
         *
         * @return true if this storage supports hotplug, false otherwise
         */
        bool isHotpluggable() const;

       /**
        * Retrieves this drives size in bytes.
        *
        * @return the size of this drive
        */
       qulonglong size() const;

       /**
        * Indicates if the storage device is currently in use
        * i.e. if at least one child storage access is
        * mounted
        *
        * @return true if at least one child storage access is mounted
        */
       bool isInUse() const;

    protected:
        /**
         * @internal
         */
        StorageDrive(StorageDrivePrivate &dd, QObject *backendObject);
    };
}

#endif // SOLID_STORAGEDRIVE_H
