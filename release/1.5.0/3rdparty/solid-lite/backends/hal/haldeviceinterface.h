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

#ifndef SOLID_BACKENDS_HAL_DEVICEINTERFACE_H
#define SOLID_BACKENDS_HAL_DEVICEINTERFACE_H

#include <solid-lite/ifaces/deviceinterface.h>
#include "haldevice.h"

#include <QObject>
#include <QStringList>

namespace Solid
{
namespace Backends
{
namespace Hal
{
class DeviceInterface : public QObject, virtual public Solid::Ifaces::DeviceInterface
{
    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::DeviceInterface)
public:
    DeviceInterface(HalDevice *device);
    virtual ~DeviceInterface();

protected:
    HalDevice *m_device;

public:
    inline static QStringList toStringList(Solid::DeviceInterface::Type type)
    {
        QStringList list;

        switch(type)
        {
        case Solid::DeviceInterface::GenericInterface:
            // Doesn't exist with HAL
            break;
        //case Solid::DeviceInterface::Processor:
        //    list << "processor";
        //    break;
        case Solid::DeviceInterface::Block:
            list << "block";
            break;
        case Solid::DeviceInterface::StorageAccess:
            // Doesn't exist with HAL, but let's assume volume always cover this type
            list << "volume";
            break;
        case Solid::DeviceInterface::StorageDrive:
            list << "storage";
            break;
        case Solid::DeviceInterface::OpticalDrive:
            list << "storage.cdrom";
            break;
        case Solid::DeviceInterface::StorageVolume:
            list << "volume";
            break;
        case Solid::DeviceInterface::OpticalDisc:
            list << "volume.disc";
            break;
        //case Solid::DeviceInterface::Camera:
        //    list << "camera";
        //    break;
        case Solid::DeviceInterface::PortableMediaPlayer:
            list << "portable_audio_player";
            break;
        /*case Solid::DeviceInterface::NetworkInterface:
            list << "net";
            break;
        case Solid::DeviceInterface::AcAdapter:
            list << "ac_adapter";
            break;
        case Solid::DeviceInterface::Battery:
            list << "battery";
            break;
        case Solid::DeviceInterface::Button:
            list << "button";
            break;
        case Solid::DeviceInterface::AudioInterface:
            list << "alsa" << "oss";
            break;
        case Solid::DeviceInterface::DvbInterface:
            list << "dvb";
            break;
        case Solid::DeviceInterface::Video:
            list << "video4linux";
            break;
        case Solid::DeviceInterface::SerialInterface:
            list << "serial";
            break;
        case Solid::DeviceInterface::SmartCardReader:
            list << "smart_card_reader";
        case Solid::DeviceInterface::InternetGateway:
            list << "internet_gateway";
        case Solid::DeviceInterface::NetworkShare:
            list << "networkshare";
            break;
        */
        case Solid::DeviceInterface::Unknown:
            break;
        case Solid::DeviceInterface::Last:
            break;
        }

        return list;
    }

    inline static Solid::DeviceInterface::Type fromString(const QString &capability)
    {
        /*if (capability == "processor")
            return Solid::DeviceInterface::Processor;
        else */if (capability == "block")
            return Solid::DeviceInterface::Block;
        else if (capability == "storage")
            return Solid::DeviceInterface::StorageDrive;
        else if (capability == "storage.cdrom")
            return Solid::DeviceInterface::OpticalDrive;
        else if (capability == "volume")
            return Solid::DeviceInterface::StorageVolume;
        else if (capability == "volume.disc")
            return Solid::DeviceInterface::OpticalDisc;
        /*else if (capability == "camera")
            return Solid::DeviceInterface::Camera; */
        else if (capability == "portable_audio_player")
            return Solid::DeviceInterface::PortableMediaPlayer;
        /*else if (capability == "net")
            return Solid::DeviceInterface::NetworkInterface;
        else if (capability == "ac_adapter")
            return Solid::DeviceInterface::AcAdapter;
        else if (capability == "battery")
            return Solid::DeviceInterface::Battery;
        else if (capability == "button")
            return Solid::DeviceInterface::Button;
        else if (capability == "alsa" || capability == "oss")
            return Solid::DeviceInterface::AudioInterface;
        else if (capability == "dvb")
            return Solid::DeviceInterface::DvbInterface;
        else if (capability == "video4linux")
            return Solid::DeviceInterface::Video;
        else if (capability == "serial")
            return Solid::DeviceInterface::SerialInterface;
        else if (capability == "smart_card_reader")
            return Solid::DeviceInterface::SmartCardReader;
        else if (capability == "networkshare")
            return Solid::DeviceInterface::NetworkShare;*/
        else
            return Solid::DeviceInterface::Unknown;
    }
};
}
}
}

#endif // SOLID_BACKENDS_HAL_DEVICEINTERFACE_H
