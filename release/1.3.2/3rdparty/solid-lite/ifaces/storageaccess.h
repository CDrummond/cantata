/*
    Copyright 2007 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_IFACES_STORAGEACCESS_H
#define SOLID_IFACES_STORAGEACCESS_H

#include <solid-lite/ifaces/deviceinterface.h>
#include <solid-lite/storageaccess.h>

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
    class StorageAccess : virtual public DeviceInterface
    {
    public:
        /**
         * Destroys a StorageVolume object.
         */
        virtual ~StorageAccess();


        /**
         * Indicates if this volume is mounted.
         *
         * @return true if the volume is mounted
         */
        virtual bool isAccessible() const = 0;

        /**
         * Retrieves the absolute path of this volume mountpoint.
         *
         * @return the absolute path to the mount point if the volume is
         * mounted, QString() otherwise
         */
        virtual QString filePath() const = 0;

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
         * Mounts the volume.
         *
         * @return the job handling the operation
         */
        virtual bool setup() = 0;

        /**
         * Unmounts the volume.
         *
         * @return the job handling the operation
         */
        virtual bool teardown() = 0;

    protected:
    //Q_SIGNALS:
        /**
         * This signal is emitted when the mount state of this device
         * has changed.
         *
         * @param newState true if the volume is mounted, false otherwise
         * @param udi the UDI of the volume
         */
        virtual void accessibilityChanged(bool accessible, const QString &udi) = 0;

        /**
         * This signal is emitted when the mount state of this device
         * has changed.
         *
         * @param newState true if the volume is mounted, false otherwise
         * @param udi the UDI of the volume
         */
        virtual void setupDone(Solid::ErrorType error, QVariant resultData, const QString &udi) = 0;

        /**
         * This signal is emitted when the mount state of this device
         * has changed.
         *
         * @param newState true if the volume is mounted, false otherwise
         * @param udi the UDI of the volume
         */
        virtual void teardownDone(Solid::ErrorType error, QVariant resultData, const QString &udi) = 0;

        /**
         * This signal is emitted when a setup of this device is requested.
         * The signal might be spontaneous i.e. it can be triggered by
         * another process.
         *
         * @param udi the UDI of the volume
         */
        virtual void setupRequested(const QString &udi) = 0;

        /**
         * This signal is emitted when a teardown of this device is requested.
         * The signal might be spontaneous i.e. it can be triggered by
         * another process
         *
         * @param udi the UDI of the volume
         */
        virtual void teardownRequested(const QString &udi) = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::StorageAccess, "org.kde.Solid.Ifaces.StorageAccess/0.1")

#endif
