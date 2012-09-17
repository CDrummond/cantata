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

#ifndef SOLID_STORAGEACCESS_H
#define SOLID_STORAGEACCESS_H

#include <solid-lite/solid_export.h>

#include <solid-lite/solidnamespace.h>
#include <solid-lite/deviceinterface.h>
#include <QtCore/QVariant>

namespace Solid
{
    class StorageAccessPrivate;
    class Device;

    /**
     * This device interface is available on volume devices to access them
     * (i.e. mount or unmount them).
     *
     * A volume is anything that can contain data (partition, optical disc,
     * memory card). It's a particular kind of block device.
     */
    class SOLID_EXPORT StorageAccess : public DeviceInterface
    {
        Q_OBJECT
        Q_PROPERTY(bool accessible READ isAccessible)
        Q_PROPERTY(QString filePath READ filePath)
        Q_PROPERTY(bool ignored READ isIgnored)
        Q_DECLARE_PRIVATE(StorageAccess)
        friend class Device;

    private:
        /**
         * Creates a new StorageAccess object.
         * You generally won't need this. It's created when necessary using
         * Device::as().
         *
         * @param backendObject the device interface object provided by the backend
         * @see Solid::Device::as()
         */
        explicit StorageAccess(QObject *backendObject);

    public:
        /**
         * Destroys a StorageAccess object.
         */
        virtual ~StorageAccess();


        /**
         * Get the Solid::DeviceInterface::Type of the StorageAccess device interface.
         *
         * @return the StorageVolume device interface type
         * @see Solid::Ifaces::Enums::DeviceInterface::Type
         */
        static Type deviceInterfaceType() { return DeviceInterface::StorageAccess; }


        /**
         * Indicates if this volume is mounted.
         *
         * @return true if the volume is mounted
         */
        bool isAccessible() const;

        /**
         * Retrieves the absolute path of this volume mountpoint.
         *
         * @return the absolute path to the mount point if the volume is
         * mounted, QString() otherwise
         */
        QString filePath() const;

        /**
         * Indicates if this volume should be ignored by applications.
         *
         * If it should be ignored, it generally means that it should be
         * invisible to the user. It's useful for firmware partitions or
         * OS reinstall partitions on some systems.
         *
         * @return true if the volume should be ignored
         */
        bool isIgnored() const;

        /**
         * Mounts the volume.
         *
         * @return false if the operation is not supported, true if the
         * operation is attempted
         */
        bool setup();

        /**
         * Unmounts the volume.
         *
         * @return false if the operation is not supported, true if the
         * operation is attempted
         */
        bool teardown();

    Q_SIGNALS:
        /**
         * This signal is emitted when the accessiblity of this device
         * has changed.
         *
         * @param accessible true if the volume is accessible, false otherwise
         * @param udi the UDI of the volume
         */
        void accessibilityChanged(bool accessible, const QString &udi);

        /**
         * This signal is emitted when the attempted setting up of this
         * device is completed. The signal might be spontaneous i.e.
         * it can be triggered by another process.
         *
         * @param error type of error that occurred, if any
         * @param errorData more information about the error, if any
         * @param udi the UDI of the volume
         */
        void setupDone(Solid::ErrorType error, QVariant errorData, const QString &udi);

        /**
         * This signal is emitted when the attempted tearing down of this
         * device is completed. The signal might be spontaneous i.e.
         * it can be triggered by another process.
         *
         * @param error type of error that occurred, if any
         * @param errorData more information about the error, if any
         * @param udi the UDI of the volume
         */
        void teardownDone(Solid::ErrorType error, QVariant errorData, const QString &udi);

        /**
         * This signal is emitted when a setup of this device is requested.
         * The signal might be spontaneous i.e. it can be triggered by
         * another process.
         *
         * @param udi the UDI of the volume
         */
        void setupRequested(const QString &udi);

        /**
         * This signal is emitted when a teardown of this device is requested.
         * The signal might be spontaneous i.e. it can be triggered by
         * another process
         *
         * @param udi the UDI of the volume
         */
        void teardownRequested(const QString &udi);

    protected:
        /**
         * @internal
         */
        StorageAccess(StorageAccessPrivate &dd, QObject *backendObject);
    };
}

#endif
