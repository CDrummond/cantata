/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>

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

#ifndef UDISKS2STORAGEVOLUME_H
#define UDISKS2STORAGEVOLUME_H

#include <ifaces/storagevolume.h>
#include "udisksblock.h"


namespace Solid
{
namespace Backends
{
namespace UDisks2
{

class StorageVolume: public Block, virtual public Solid::Ifaces::StorageVolume
{
    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::StorageVolume)

public:
    StorageVolume(Device *device);
    virtual ~StorageVolume();

    virtual QString encryptedContainerUdi() const;
    virtual qulonglong size() const;
    virtual QString uuid() const;
    virtual QString label() const;
    virtual QString fsType() const;
    virtual Solid::StorageVolume::UsageType usage() const;
    virtual bool isIgnored() const;
};

}
}
}

#endif // UDISKS2STORAGEVOLUME_H
