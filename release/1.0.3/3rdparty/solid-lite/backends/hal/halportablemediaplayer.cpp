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

#include "halportablemediaplayer.h"

using namespace Solid::Backends::Hal;

PortableMediaPlayer::PortableMediaPlayer(HalDevice *device)
    : DeviceInterface(device)
{

}

PortableMediaPlayer::~PortableMediaPlayer()
{

}

QStringList PortableMediaPlayer::supportedProtocols() const
{
    return m_device->prop("portable_audio_player.access_method.protocols").toStringList();
}

QStringList PortableMediaPlayer::supportedDrivers(QString protocol) const
{
    QStringList drivers = m_device->prop("portable_audio_player.access_method.drivers").toStringList();
    if(protocol.isNull())
        return drivers;
    QStringList returnedDrivers;
    QString temp;
    for(int i = 0; i < drivers.size(); i++)
    {
        temp = drivers.at(i);
        if(m_device->prop("portable_audio_player." + temp + ".protocol") == protocol)
            returnedDrivers << temp;
    }
    return returnedDrivers;
}

QVariant PortableMediaPlayer::driverHandle(const QString &driver) const
{
    if (driver=="mtp") {
        return m_device->prop("usb.serial");
    }
    // TODO: Fill in the blank for other drivers

    return QVariant();
}

//#include "backends/hal/halportablemediaplayer.moc"
