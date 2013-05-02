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

#ifndef SOLID_OPTICALDRIVE_H
#define SOLID_OPTICALDRIVE_H

#include <QtCore/QList>
#include <QtCore/QVariant>

#include <solid-lite/solid_export.h>
#include <solid-lite/solidnamespace.h>

#include <solid-lite/storagedrive.h>

namespace Solid
{
    class OpticalDrivePrivate;
    class Device;

    /**
     * This device interface is available on CD-R*,DVD*,Blu-Ray,HD-DVD drives.
     *
     * An OpticalDrive is a storage that can handle optical discs.
     */
    class SOLID_EXPORT OpticalDrive : public StorageDrive
    {
        Q_OBJECT
        Q_ENUMS(MediumType)
        Q_FLAGS(MediumTypes)
        Q_PROPERTY(MediumTypes supportedMedia READ supportedMedia)
        Q_PROPERTY(int readSpeed READ readSpeed)
        Q_PROPERTY(int writeSpeed READ writeSpeed)
        Q_PROPERTY(QList<int> writeSpeeds READ writeSpeeds)
        Q_DECLARE_PRIVATE(OpticalDrive)
        friend class Device;

    public:
        /**
         * This enum type defines the type of medium an optical drive supports.
         *
         * - Cdr : A Recordable Compact Disc (CD-R)
         * - Cdrw : A ReWritable Compact Disc (CD-RW)
         * - Dvd : A Digital Versatile Disc (DVD)
         * - Dvdr : A Recordable Digital Versatile Disc (DVD-R)
         * - Dvdrw : A ReWritable Digital Versatile Disc (DVD-RW)
         * - Dvdram : A Random Access Memory Digital Versatile Disc (DVD-RAM)
         * - Dvdplusr : A Recordable Digital Versatile Disc (DVD+R)
         * - Dvdplusrw : A ReWritable Digital Versatile Disc (DVD+RW)
         * - Dvdplusdl : A Dual Layer Digital Versatile Disc (DVD+R DL)
         * - Dvdplusdlrw : A Dual Layer Digital Versatile Disc (DVD+RW DL)
         * - Bd : A Blu-ray Disc (BD)
         * - Bdr : A Blu-ray Disc Recordable (BD-R)
         * - Bdre : A Blu-ray Disc Recordable and Eraseable (BD-RE)
         * - HdDvd : A High Density Digital Versatile Disc (HD DVD)
         * - HdDvdr : A High Density Digital Versatile Disc Recordable (HD DVD-R)
         * - HdDvdrw : A High Density Digital Versatile Disc ReWritable (HD DVD-RW)
         */
        enum MediumType { Cdr=0x00001, Cdrw=0x00002, Dvd=0x00004, Dvdr=0x00008,
                          Dvdrw=0x00010, Dvdram=0x00020, Dvdplusr=0x00040,
                          Dvdplusrw=0x00080, Dvdplusdl=0x00100, Dvdplusdlrw=0x00200,
                          Bd=0x00400, Bdr=0x00800, Bdre=0x01000,
                          HdDvd=0x02000, HdDvdr=0x04000, HdDvdrw=0x08000 };

        /**
         * This type stores an OR combination of MediumType values.
         */
        Q_DECLARE_FLAGS(MediumTypes, MediumType)


    private:
        /**
         * Creates a new OpticalDrive object.
         * You generally won't need this. It's created when necessary using
         * Device::as().
         *
         * @param backendObject the device interface object provided by the backend
         * @see Solid::Device::as()
         */
        explicit OpticalDrive(QObject *backendObject);

    public:
        /**
         * Destroys an OpticalDrive object.
         */
        virtual ~OpticalDrive();


        /**
         * Get the Solid::DeviceInterface::Type of the OpticalDrive device interface.
         *
         * @return the OpticalDrive device interface type
         * @see Solid::Ifaces::Enums::DeviceInterface::Type
         */
        static Type deviceInterfaceType() { return DeviceInterface::OpticalDrive; }


        /**
         * Retrieves the medium types this drive supports.
         *
         * @return the flag set indicating the supported medium types
         */
        MediumTypes supportedMedia() const;

        /**
         * Retrieves the maximum read speed of this drive in kilobytes per second.
         *
         * @return the maximum read speed
         */
        int readSpeed() const;

        /**
         * Retrieves the maximum write speed of this drive in kilobytes per second.
         *
         * @return the maximum write speed
         */
        int writeSpeed() const;

        /**
         * Retrieves the list of supported write speeds of this drive in
         * kilobytes per second.
         *
         * @return the list of supported write speeds
         */
        QList<int> writeSpeeds() const;

        /**
         * Ejects any disc that could be contained in this drive.
         * If this drive is empty, but has a tray it'll be opened.
         *
         * @return the status of the eject operation
         */
        bool eject();

    Q_SIGNALS:
        /**
         * This signal is emitted when the eject button is pressed
         * on the drive.
         *
         * Please note that some (broken) drives doesn't report this event.
         * @param udi the UDI of the drive
         */
        void ejectPressed(const QString &udi);

        /**
         * This signal is emitted when the attempted eject process on this
         * drive is completed. The signal might be spontaneous, i.e.
         * it can be triggered by another process.
         *
         * @param error type of error that occurred, if any
         * @param errorData more information about the error, if any
         * @param udi the UDI of the volume
         */
        void ejectDone(Solid::ErrorType error, QVariant errorData, const QString &udi);

        /**
         * This signal is emitted when eject on this drive is
         * requested. The signal might be spontaneous, i.e. it
         * can be triggered by another process.
         *
         * @param udi the UDI of the volume
         */
        void ejectRequested(const QString &udi);

    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Solid::OpticalDrive::MediumTypes)

#endif // SOLID_OPTICALDRIVE_H
