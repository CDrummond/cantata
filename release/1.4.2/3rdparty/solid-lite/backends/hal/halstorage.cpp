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

#include "halstorage.h"

using namespace Solid::Backends::Hal;

Storage::Storage(HalDevice *device)
    : Block(device)
{

}

Storage::~Storage()
{

}

Solid::StorageDrive::Bus Storage::bus() const
{
    QString bus = m_device->prop("storage.bus").toString();

    if (bus=="ide")
    {
        return Solid::StorageDrive::Ide;
    }
    else if (bus=="usb")
    {
        return Solid::StorageDrive::Usb;
    }
    else if (bus=="ieee1394")
    {
        return Solid::StorageDrive::Ieee1394;
    }
    else if (bus=="scsi")
    {
        return Solid::StorageDrive::Scsi;
    }
    else if (bus=="sata")
    {
        return Solid::StorageDrive::Sata;
    }
    else
    {
        return Solid::StorageDrive::Platform;
    }
}

Solid::StorageDrive::DriveType Storage::driveType() const
{
    QString type = m_device->prop("storage.drive_type").toString();

    if (type=="disk")
    {
        return Solid::StorageDrive::HardDisk;
    }
    else if (type=="cdrom")
    {
        return Solid::StorageDrive::CdromDrive;
    }
    else if (type=="floppy")
    {
        return Solid::StorageDrive::Floppy;
    }
    else if (type=="tape")
    {
        return Solid::StorageDrive::Tape;
    }
    else if (type=="compact_flash")
    {
        return Solid::StorageDrive::CompactFlash;
    }
    else if (type=="memory_stick")
    {
        return Solid::StorageDrive::MemoryStick;
    }
    else if (type=="smart_media")
    {
        return Solid::StorageDrive::SmartMedia;
    }
    else if (type=="sd_mmc")
    {
        return Solid::StorageDrive::SdMmc;
    }
    else
    {
        return Solid::StorageDrive::HardDisk;
    }
}

bool Storage::isRemovable() const
{
    return m_device->prop("storage.removable").toBool();
}

bool Storage::isHotpluggable() const
{
    return m_device->prop("storage.hotpluggable").toBool();
}

qulonglong Storage::size() const
{
  return m_device->prop("storage.size").toULongLong();
}

//#include "backends/hal/halstorage.moc"
