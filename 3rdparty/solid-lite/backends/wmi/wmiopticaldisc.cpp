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

#include "wmiopticaldisc.h"


#include <QDir>

using namespace Solid::Backends::Wmi;

OpticalDisc::OpticalDisc(WmiDevice *device)
    : Volume(device)
{
    m_logicalDisk = WmiDevice::win32LogicalDiskByDriveLetter(m_device->property("Drive").toString());
}

OpticalDisc::~OpticalDisc()
{

}


Solid::OpticalDisc::ContentTypes OpticalDisc::availableContent() const
{
    Solid::OpticalDisc::ContentTypes content;

    QDir dir(m_device->property("Drive").toString());
    QStringList files = dir.entryList();
    if(files.length()>0)
        if(files[0].endsWith(".cda"))
            content |= Solid::OpticalDisc::Audio;

    return content;
}

Solid::OpticalDisc::DiscType OpticalDisc::discType() const
{
    QString type = m_logicalDisk.getProperty("FileSystem").toString();

    if (type == "CDFS")
    {
        return Solid::OpticalDisc::CdRom;
    }
//    else if (type == "CdRomWrite")
//    {
//        return Solid::OpticalDisc::CdRecordable;
//    }
    else if (type == "UDF")
    {
        return Solid::OpticalDisc::DvdRom;
    }
//    else if (type == "DVDRomWrite")
//    {
//        return Solid::OpticalDisc::DvdRecordable;
//    }
    else
    {
        qDebug()<<"Solid::OpticalDisc::DiscType OpticalDisc::discType(): Unknown Type"<<type;
        return Solid::OpticalDisc::UnknownDiscType;
    }
}

bool OpticalDisc::isAppendable() const
{
    return false;
}

bool OpticalDisc::isBlank() const
{
    ushort val = m_device->property("FileSystemFlagsEx").toUInt();
    if(val == 0)
        return true;
    return false;
}

bool OpticalDisc::isRewritable() const
{
    //TODO:
    return capacity()>0 && isWriteable();
}

qulonglong OpticalDisc::capacity() const
{
    return m_device->property("Size").toULongLong();
}

bool OpticalDisc::isWriteable() const
{
    ushort val = m_device->property("FileSystemFlagsEx").toUInt();
    if(val == 0)
        return true;
    return !val & 0x80001;//read only
}

#include "backends/wmi/wmiopticaldisc.moc"
