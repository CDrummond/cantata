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

#ifndef UDISKS2OPTICALDRIVE_H
#define UDISKS2OPTICALDRIVE_H

#include <solid-lite/ifaces/opticaldrive.h>
#include "udisksstoragedrive.h"

namespace Solid
{
namespace Backends
{
namespace UDisks2
{

class OpticalDrive: public StorageDrive, virtual public Solid::Ifaces::OpticalDrive
{
    Q_OBJECT
    Q_INTERFACES(Solid::Ifaces::OpticalDrive)

public:
    OpticalDrive(Device *device);
    virtual ~OpticalDrive();

Q_SIGNALS:
    void ejectPressed(const QString &udi);
    void ejectDone(Solid::ErrorType error, QVariant errorData, const QString &udi);
    void ejectRequested(const QString &udi);

public:
    virtual bool eject();
    virtual QList<int> writeSpeeds() const;
    virtual int writeSpeed() const;
    virtual int readSpeed() const;
    virtual Solid::OpticalDrive::MediumTypes supportedMedia() const;

private Q_SLOTS:
    void slotDBusReply(const QDBusMessage &reply);
    void slotDBusError(const QDBusError &error);

    void slotEjectRequested();
    void slotEjectDone(int error, const QString &errorString);

    void slotChanged();

private:
    void initReadWriteSpeeds() const;

    bool m_ejectInProgress;

    // read/write speeds
    mutable int m_readSpeed;
    mutable int m_writeSpeed;
    mutable QList<int> m_writeSpeeds;
    mutable bool m_speedsInit;
};

}
}
}

#endif // UDISKS2OPTICALDRIVE_H
