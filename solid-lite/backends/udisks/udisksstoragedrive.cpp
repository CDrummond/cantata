/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010 Lukas Tinkl <ltinkl@redhat.com>

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

#include "udisksstoragedrive.h"

#include <QDebug>

using namespace Solid::Backends::UDisks;

UDisksStorageDrive::UDisksStorageDrive(UDisksDevice* device)
    : Block(device)
{

}

UDisksStorageDrive::~UDisksStorageDrive()
{

}

qulonglong UDisksStorageDrive::size() const
{
    return m_device->prop("DeviceSize").toULongLong();
}

bool UDisksStorageDrive::isHotpluggable() const
{
    return m_device->prop("DriveCanDetach").toBool();
}

bool UDisksStorageDrive::isRemovable() const
{
    return m_device->prop("DeviceIsRemovable").toBool() ||
            !m_device->prop( "DeviceIsSystemInternal" ).toBool();
}

Solid::StorageDrive::DriveType UDisksStorageDrive::driveType() const
{
    const QStringList mediaTypes = m_device->prop("DriveMediaCompatibility").toStringList();
    bool isHardDisk = m_device->prop( "DeviceIsSystemInternal" ).toBool();

    if ( isHardDisk )
    {
        return Solid::StorageDrive::HardDisk;
    }
    else if ( !mediaTypes.filter( "optical" ).isEmpty() ) // optical disks
    {
        return Solid::StorageDrive::CdromDrive;
    }
    else if ( mediaTypes.contains( "floppy" ) )
    {
        return Solid::StorageDrive::Floppy;
    }
#if 0 // TODO add to Solid
    else if ( mediaTypes.contains( "floppy_jaz" ) )
    {
        return Solid::StorageDrive::Jaz;
    }
    else if ( mediaTypes.contains( "floppy_zip" ) )
    {
        return Solid::StorageDrive::Zip;
    }
#endif
    /*
    else if (type=="tape")      // FIXME: DK doesn't know about tapes
    {
        return Solid::StorageDrive::Tape;
    }
    */
#if 0 // TODO add to Solid
    else if ( mediaTypes.contains( "flash" ) )
    {
        return Solid::StorageDrive::Flash;
    }
#endif
    else if ( mediaTypes.contains( "flash_cf" ) )
    {
        return Solid::StorageDrive::CompactFlash;
    }
    else if ( mediaTypes.contains( "flash_ms" ) )
    {
        return Solid::StorageDrive::MemoryStick;
    }
    else if ( mediaTypes.contains( "flash_sm" ) )
    {
        return Solid::StorageDrive::SmartMedia;
    }
    else if ( mediaTypes.contains( "flash_sd" ) || mediaTypes.contains( "flash_sdhc" ) || mediaTypes.contains( "flash_mmc" ) )
    {
        return Solid::StorageDrive::SdMmc;
    }
    // FIXME: DK doesn't know about xD cards either
    else
    {
        return Solid::StorageDrive::HardDisk;
    }
}

Solid::StorageDrive::Bus UDisksStorageDrive::bus() const
{
    const QString bus = m_device->prop( "DriveConnectionInterface" ).toString();

    if ( bus == "ata" || bus == "ata_parallel" ) // parallel (classical) ATA
    {
        return Solid::StorageDrive::Ide;
    }
    else if ( bus == "usb" )
    {
        return Solid::StorageDrive::Usb;
    }
    else if ( bus == "firewire" )
    {
        return Solid::StorageDrive::Ieee1394;
    }
    else if ( bus == "scsi" )
    {
        return Solid::StorageDrive::Scsi;
    }
    else if ( bus == "ata_serial" || bus == "ata_serial_esata" ) // serial ATA
    {
        return Solid::StorageDrive::Sata;
    }
#if 0  // TODO add these to Solid
    else if ( bus == "sdio" )
    {
        return Solid::StorageDrive::SDIO;
    }
    else if ( bus == "virtual" )
    {
        return Solid::StorageDrive::Virtual;
    }
#endif
    else
        return Solid::StorageDrive::Platform;
}

