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

#ifndef SOLID_BACKENDS_WMI_CDROM_H
#define SOLID_BACKENDS_WMI_CDROM_H

#include <solid/ifaces/opticaldrive.h>
#include "wmistorage.h"

#include <QtCore/QProcess>

namespace Solid
{
namespace Backends
{
namespace Wmi
{
class Cdrom : public Storage, virtual public Solid::Ifaces::OpticalDrive
{
    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::OpticalDrive)

public:
    Cdrom(WmiDevice *device);
    virtual ~Cdrom();

    virtual Solid::OpticalDrive::MediumTypes supportedMedia() const;
    virtual int readSpeed() const;
    virtual int writeSpeed() const;
    virtual QList<int> writeSpeeds() const;
    virtual bool eject();

Q_SIGNALS:
    void ejectPressed(const QString &udi);
    void ejectDone(Solid::ErrorType error, QVariant errorData, const QString &udi);

private Q_SLOTS:
    void slotCondition(const QString &name, const QString &reason);
    void slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    bool callWmiDriveEject();

    bool m_ejectInProgress;
    QProcess *m_process;
};
}
}
}

#endif // SOLID_BACKENDS_WMI_CDROM_H
