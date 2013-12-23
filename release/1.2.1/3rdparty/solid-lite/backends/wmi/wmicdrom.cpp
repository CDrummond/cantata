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

#include "wmicdrom.h"

#include <QtCore/QStringList>


using namespace Solid::Backends::Wmi;

Cdrom::Cdrom(WmiDevice *device)
    : Storage(device), m_ejectInProgress(false)
{
    connect(device, SIGNAL(conditionRaised(QString,QString)),
             this, SLOT(slotCondition(QString,QString)));
}

Cdrom::~Cdrom()
{

}


Solid::OpticalDrive::MediumTypes Cdrom::supportedMedia() const
{
    Solid::OpticalDrive::MediumTypes supported;

    QString type = m_device->property("MediaType").toString();
    if (type == "CdRomOnly" || type == "CD-ROM")
    {
        supported |= Solid::OpticalDrive::Cdr;
    }
    else if (type == "CdRomWrite")
    {
        supported |= Solid::OpticalDrive::Cdr|Solid::OpticalDrive::Cdrw;
    }
    else if (type == "DVDRomOnly")
    {
        supported |= Solid::OpticalDrive::Dvd;
    }
    else if (type == "DVDRomWrite" || type == "DVD Writer")
    {
        supported |= Solid::OpticalDrive::Dvd|Solid::OpticalDrive::Dvdr|Solid::OpticalDrive::Dvdrw;
    }

    return supported;
}

int Cdrom::readSpeed() const
{
    return m_device->property("TransferRate").toInt();
}

int Cdrom::writeSpeed() const
{
    return m_device->property("TransferRate").toInt();
}

QList<int> Cdrom::writeSpeeds() const
{
    QList<int> speeds;
    return speeds;
}

void Cdrom::slotCondition(const QString &name, const QString &/*reason */)
{
    if (name == "EjectPressed")
    {
        emit ejectPressed(m_device->udi());
    }
}

bool Cdrom::eject()
{
    if (m_ejectInProgress) {
        return false;
    }
    m_ejectInProgress = true;

    return callWmiDriveEject();
}

bool Cdrom::callWmiDriveEject()
{
//    QString udi = m_device->udi();
//    QString interface = "org.freedesktop.Wmi.Device.Storage";

    // HACK: Eject doesn't work on cdrom drives when there's a mounted disc,
    // let's try to workaround this by calling a child volume...
    // if (m_device->property("storage.removable.media_available").toBool()) {
        // QDBusInterface manager("org.freedesktop.Wmi",
                               // "/org/freedesktop/Wmi/Manager",
                               // "org.freedesktop.Wmi.Manager",
                               // QDBusConnection::systemBus());

        // QDBusReply<QStringList> reply = manager.call("FindDeviceStringMatch", "info.parent", udi);

        // if (reply.isValid())
        // {
            // QStringList udis = reply;
            // if (!udis.isEmpty()) {
                // udi = udis[0];
                // interface = "org.freedesktop.Wmi.Device.Volume";
            // }
        // }
    // }

    // QDBusConnection c = QDBusConnection::systemBus();
    // QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Wmi", udi,
                                                      // interface, "Eject");

    // msg << QStringList();


    // return c.callWithCallback(msg, this,
                              // SLOT(slotDBusReply(QDBusMessage)),
                              // SLOT(slotDBusError(QDBusError)));
    return false;
}

void Solid::Backends::Wmi::Cdrom::slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_ejectInProgress) {
        m_ejectInProgress = false;

        if (exitCode==0) {
            emit ejectDone(Solid::NoError, QVariant(), m_device->udi());
        } else {
            emit ejectDone(Solid::UnauthorizedOperation,
                           m_process->readAllStandardError(),
                           m_device->udi());
        }
    }

    delete m_process;
}

#include "backends/wmi/wmicdrom.moc"
