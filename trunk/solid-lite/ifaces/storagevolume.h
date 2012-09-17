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

#ifndef SOLID_IFACES_STORAGEVOLUME_H
#define SOLID_IFACES_STORAGEVOLUME_H

#include <solid-lite/ifaces/block.h>
#include <solid-lite/storagevolume.h>

namespace Solid
{
namespace Ifaces
{
    /**
     * This device interface is available on volume devices.
     *
     * A volume is anything that can contain data (partition, optical disc,
     * memory card). It's a particular kind of block device.
     */
    class StorageVolume : virtual public Block
    {
    public:
        /**
         * Destroys a StorageVolume object.
         */
        virtual ~StorageVolume();


        /**
         * Indicates if this volume should be ignored by applications.
         *
         * If it should be ignored, it generally means that it should be
         * invisible to the user. It's useful for firmware partitions or
         * OS reinstall partitions on some systems.
         *
         * @return true if the volume should be ignored
         */
        virtual bool isIgnored() const = 0;

        /**
         * Retrieves the type of use for this volume (for example filesystem).
         *
         * @return the usage type
         * @see Solid::StorageVolume::UsageType
         */
        virtual Solid::StorageVolume::UsageType usage() const = 0;

        /**
         * Retrieves the filesystem type of this volume.
         *
         * @return the filesystem type if applicable, QString() otherwise
         */
        virtual QString fsType() const = 0;

        /**
         * Retrieves this volume label.
         *
         * @return the volume lavel if available, QString() otherwise
         */
        virtual QString label() const = 0;

        /**
         * Retrieves this volume Universal Unique IDentifier (UUID).
         *
         * You can generally assume that this identifier is unique with reasonable
         * confidence. Except if the volume UUID has been forged to intentionally
         * provoke a collision, the probability to have two volumes having the same
         * UUID is low.
         *
         * @return the Universal Unique IDentifier if available, QString() otherwise
         */
        virtual QString uuid() const = 0;

        /**
         * Retrieves this volume size in bytes.
         *
         * @return the size of this volume
         */
        virtual qulonglong size() const = 0;

        /**
         * Retrieves the crypto container UDI of this volume.
         *
         * @return the encrypted volume UDI containing the current volume if appliable,
         * an empty string otherwise
         */
        virtual QString encryptedContainerUdi() const = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::StorageVolume, "org.kde.Solid.Ifaces.StorageVolume/0.1")

#endif // SOLID_IFACES_STORAGEVOLUME_H
