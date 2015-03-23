/*
    Copyright 2006 Davide Bettio <davide.bettio@kdemail.net>
    Copyright 2007 Jeff Mitchell <kde-dev@emailgoeshere.com>

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

#ifndef SOLID_IFACES_PORTABLEMEDIAPLAYER_H
#define SOLID_IFACES_PORTABLEMEDIAPLAYER_H

#include <solid-lite/ifaces/deviceinterface.h>
#include <solid-lite/portablemediaplayer.h>

namespace Solid
{
namespace Ifaces
{
    /**
     * This class implements Portable Media Player device interface and represents
     * a portable media player attached to the system.
     * A portable media player is a portable device able to play multimedia files.
     * Some of them have even recording capabilities.
     * @author Davide Bettio <davide.bettio@kdemail.net>
     */
    class PortableMediaPlayer : virtual public DeviceInterface
    {
    public:
        /**
         * Destroys a portable media player object.
         */
        virtual ~PortableMediaPlayer();

        /**
         * Retrieves known protocols this device can speak.  This list may be dependent
         * on installed device driver libraries.
         *
         * Possible protocols:
         *  * storage - filesystem-based device: can browse and play media files stored
         *              on its volume. iPod-like devices can have both storage and ipod protocol
         *              set, you should use more specific (ipod) protocol in this case.
         *  * ipod - iPod-like device where media files are stored on filesystem, but these need
         *           an entry in device database in order to be playable.
         *  * mtp - Media Transfer Protocol-compatible devices.
         *
         * @return a list of known protocols this device can speak
         */
        virtual QStringList supportedProtocols() const = 0;

        /**
         * Retrieves known installed device drivers that claim to handle this device
         * using the requested protocol.
         *
         * Possible drivers:
         *  * usb - device is talked to using USB. This driver alone does not specify which
         *          particular USB service/protocol should be used.
         *  * usbmux - device supports AFC (Apple File Connection) and usbmuxd daemon is ready
         *             on /var/run/usbmuxd socket on UNIX and localhost:27015 port on Windows.
         *
         * @param protocol The protocol to get drivers for. Specify empty protocol to get
         *                 drivers for all possible protocols.
         * @return a list of known device drivers that can handle this device
         */
        virtual QStringList supportedDrivers(QString protocol = QString()) const = 0;

        /**
         * Retrieves a driver specific string allowing to access the device.
         *
         * For example for the "mtp" driver it will return the serial number
         * of the device and "usbmux" driver will return 40-digit device UUID
         *
         * @return the driver specific data
         */
        virtual QVariant driverHandle(const QString &driver) const = 0;
    };
}
}

Q_DECLARE_INTERFACE(Solid::Ifaces::PortableMediaPlayer, "org.kde.Solid.Ifaces.PortableMediaPlayer/0.1")

#endif
