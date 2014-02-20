/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010-2012 Lukáš Tinkl <ltinkl@redhat.com>

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

#include "udisksstoragevolume.h"
#include "udisks2.h"

using namespace Solid::Backends::UDisks2;

StorageVolume::StorageVolume(Device *device)
    : Block(device)
{
}

StorageVolume::~StorageVolume()
{
}

QString StorageVolume::encryptedContainerUdi() const
{
    const QString path = m_device->prop("CryptoBackingDevice").value<QDBusObjectPath>().path();
    if ( path.isEmpty() || path == "/")
        return QString();
    else
        return path;
}

qulonglong StorageVolume::size() const
{
    return m_device->prop("Size").toULongLong();
}

QString StorageVolume::uuid() const
{
    return m_device->prop("IdUUID").toString();
}

QString StorageVolume::label() const
{
    QString label = m_device->prop("HintName").toString();
    if (label.isEmpty())
        label = m_device->prop("IdLabel").toString();
    if (label.isEmpty())
        label = m_device->prop("Name").toString();
    return label;
}

QString StorageVolume::fsType() const
{
    return m_device->prop("IdType").toString();
}

Solid::StorageVolume::UsageType StorageVolume::usage() const
{
    const QString usage = m_device->prop("IdUsage").toString();

    if (m_device->hasInterface(UD2_DBUS_INTERFACE_FILESYSTEM))
    {
        return Solid::StorageVolume::FileSystem;
    }
    else if (m_device->isPartitionTable())
    {
        return Solid::StorageVolume::PartitionTable;
    }
    else if (usage == "raid")
    {
        return Solid::StorageVolume::Raid;
    }
    else if (m_device->isEncryptedContainer())
    {
        return Solid::StorageVolume::Encrypted;
    }
    else if (usage == "unused" || usage.isEmpty())
    {
        return Solid::StorageVolume::Unused;
    }
    else
    {
        return Solid::StorageVolume::Other;
    }
}

bool StorageVolume::isIgnored() const
{
    const Solid::StorageVolume::UsageType usg = usage();
    return m_device->prop("HintIgnore").toBool() || m_device->isSwap() ||
            ((usg == Solid::StorageVolume::Unused || usg == Solid::StorageVolume::Other || usg == Solid::StorageVolume::PartitionTable) && !m_device->isOpticalDisc());
}
