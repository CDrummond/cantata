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

#include "halopticaldisc.h"

using namespace Solid::Backends::Hal;

OpticalDisc::OpticalDisc(HalDevice *device)
    : Volume(device)
{

}

OpticalDisc::~OpticalDisc()
{

}


Solid::OpticalDisc::ContentTypes OpticalDisc::availableContent() const
{
    Solid::OpticalDisc::ContentTypes content;

    QMap<Solid::OpticalDisc::ContentType, QString> map;
    map[Solid::OpticalDisc::Audio] = "volume.disc.has_audio";
    map[Solid::OpticalDisc::Data] = "volume.disc.has_data";
    map[Solid::OpticalDisc::VideoCd] = "volume.disc.is_vcd";
    map[Solid::OpticalDisc::SuperVideoCd] = "volume.disc.is_svcd";
    map[Solid::OpticalDisc::VideoDvd] ="volume.disc.is_videodvd";
    map[Solid::OpticalDisc::VideoBluRay] ="volume.disc.is_blurayvideo";

    foreach (const Solid::OpticalDisc::ContentType type, map.keys())
    {
        if (m_device->prop(map[type]).toBool())
        {
            content|= type;
        }
    }

    return content;
}

Solid::OpticalDisc::DiscType OpticalDisc::discType() const
{
    QString type = m_device->prop("volume.disc.type").toString();

    if (type == "cd_rom")
    {
        return Solid::OpticalDisc::CdRom;
    }
    else if (type == "cd_r")
    {
        return Solid::OpticalDisc::CdRecordable;
    }
    else if (type == "cd_rw")
    {
        return Solid::OpticalDisc::CdRewritable;
    }
    else if (type == "dvd_rom")
    {
        return Solid::OpticalDisc::DvdRom;
    }
    else if (type == "dvd_ram")
    {
        return Solid::OpticalDisc::DvdRam;
    }
    else if (type == "dvd_r")
    {
        return Solid::OpticalDisc::DvdRecordable;
    }
    else if (type == "dvd_rw")
    {
        return Solid::OpticalDisc::DvdRewritable;
    }
    else if (type == "dvd_plus_r")
    {
        return Solid::OpticalDisc::DvdPlusRecordable;
    }
    else if (type == "dvd_plus_rw")
    {
        return Solid::OpticalDisc::DvdPlusRewritable;
    }
    else if (type == "dvd_plus_r_dl")
    {
        return Solid::OpticalDisc::DvdPlusRecordableDuallayer;
    }
    else if (type == "dvd_plus_rw_dl")
    {
        return Solid::OpticalDisc::DvdPlusRewritableDuallayer;
    }
    else if (type == "bd_rom")
    {
        return Solid::OpticalDisc::BluRayRom;
    }
    else if (type == "bd_r")
    {
        return Solid::OpticalDisc::BluRayRecordable;
    }
    else if (type == "bd_re")
    {
        return Solid::OpticalDisc::BluRayRewritable;
    }
    else if (type == "hddvd_rom")
    {
        return Solid::OpticalDisc::HdDvdRom;
    }
    else if (type == "hddvd_r")
    {
        return Solid::OpticalDisc::HdDvdRecordable;
    }
    else if (type == "hddvd_rw")
    {
        return Solid::OpticalDisc::HdDvdRewritable;
    }
    else
    {
        return Solid::OpticalDisc::UnknownDiscType;
    }
}

bool OpticalDisc::isAppendable() const
{
    return m_device->prop("volume.disc.is_appendable").toBool();
}

bool OpticalDisc::isBlank() const
{
    return m_device->prop("volume.disc.is_blank").toBool();
}

bool OpticalDisc::isRewritable() const
{
    return m_device->prop("volume.disc.is_rewritable").toBool();
}

qulonglong OpticalDisc::capacity() const
{
    return m_device->prop("volume.disc.capacity").toULongLong();
}

//#include "backends/hal/halopticaldisc.moc"
