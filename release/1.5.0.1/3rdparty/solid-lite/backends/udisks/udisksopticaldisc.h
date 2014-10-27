/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010, 2011 Lukas Tinkl <ltinkl@redhat.com>

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

#ifndef OPTICALDISC_H
#define OPTICALDISC_H

#include <solid-lite/ifaces/opticaldisc.h>
#include "udisksstoragevolume.h"

namespace Solid
{
namespace Backends
{
namespace UDisks
{

class OpticalDisc : public UDisksStorageVolume, virtual public Solid::Ifaces::OpticalDisc
{

    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::OpticalDisc)

public:
    OpticalDisc(UDisksDevice *device);
    virtual ~OpticalDisc();

    virtual qulonglong capacity() const;
    virtual bool isRewritable() const;
    virtual bool isBlank() const;
    virtual bool isAppendable() const;
    virtual Solid::OpticalDisc::DiscType discType() const;
    virtual Solid::OpticalDisc::ContentTypes availableContent() const;

private slots:
    void slotChanged();

private:
    mutable bool m_needsReprobe;
    mutable Solid::OpticalDisc::ContentTypes m_cachedContent;
};

}
}
}
#endif // OPTICALDISC_H
