/*
    Copyright 2012 Patrick von Reth <vonreth@kde.org>
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

#include "wmistorage.h"
#include "wmiquery.h"

using namespace Solid::Backends::Wmi;

Storage::Storage(WmiDevice *device)
    : Block(device)
{

    if(m_device->type() == Solid::DeviceInterface::StorageDrive)
    {
        WmiQuery::Item item =  WmiDevice::win32DiskPartitionByDeviceIndex(m_device->property("DeviceID").toString());
        QString id = item.getProperty("DeviceID").toString();
        m_logicalDisk = WmiDevice::win32LogicalDiskByDiskPartitionID(id);
    }else if(m_device->type() == Solid::DeviceInterface::OpticalDrive)
    {
        QString id = m_device->property("Drive").toString();
        m_logicalDisk = WmiDevice::win32LogicalDiskByDriveLetter(id);
    }
}

Storage::~Storage()
{

}

Solid::StorageDrive::Bus Storage::bus() const
{
     if(m_device->type() == Solid::DeviceInterface::OpticalDrive)
         return Solid::StorageDrive::Platform;


    QString bus =  m_device->property("InterfaceType").toString().toLower();

    if (bus=="ide")
    {
        return Solid::StorageDrive::Ide;
    }
    else if (bus=="usb")
    {
        return Solid::StorageDrive::Usb;
    }
    else if (bus=="1394")
    {
        return Solid::StorageDrive::Ieee1394;
    }
    else if (bus=="scsi")
    {
        return Solid::StorageDrive::Scsi;
    }
//    else if (bus=="sata")//not availible http://msdn.microsoft.com/en-us/library/windows/desktop/aa394132(v=vs.85).aspx
//    {
//        return Solid::StorageDrive::Sata;
//    }
    else
    {
        return Solid::StorageDrive::Platform;
    }
}

Solid::StorageDrive::DriveType Storage::driveType() const
{
    ushort type = m_logicalDisk.getProperty("DriveType").toUInt();
    switch(type){
    case 2:
        return Solid::StorageDrive::MemoryStick;
    case 3:
        return Solid::StorageDrive::HardDisk;
    case 5:
        return Solid::StorageDrive::CdromDrive;
    default:
        return Solid::StorageDrive::HardDisk;
    }
}

bool Storage::isRemovable() const
{
    return driveType() != Solid::StorageDrive::HardDisk;
}

bool Storage::isHotpluggable() const
{
    return bus() == Solid::StorageDrive::Usb;
}

qulonglong Storage::size() const
{
    return m_device->property("Size").toULongLong();
}

//#include "backends/wmi/wmistorage.moc"
