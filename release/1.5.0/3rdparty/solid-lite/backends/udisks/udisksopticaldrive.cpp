/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010-2011 Lukas Tinkl <ltinkl@redhat.com>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <QtCore/QFile>
#include <QtCore/QDebug>

#include "udisksopticaldrive.h"
#include "udisks.h"
#include "udisksdevice.h"

using namespace Solid::Backends::UDisks;

UDisksOpticalDrive::UDisksOpticalDrive(UDisksDevice *device)
    : UDisksStorageDrive(device), m_ejectInProgress(false), m_readSpeed(0), m_writeSpeed(0), m_speedsInit(false)
{
    m_device->registerAction("eject", this,
                             SLOT(slotEjectRequested()),
                             SLOT(slotEjectDone(int,QString)));

    connect(m_device, SIGNAL(changed()), this, SLOT(slotChanged()));
}

UDisksOpticalDrive::~UDisksOpticalDrive()
{

}

bool UDisksOpticalDrive::eject()
{
    if (m_ejectInProgress)
        return false;
    m_ejectInProgress = true;
    m_device->broadcastActionRequested("eject");

    QDBusConnection c = QDBusConnection::systemBus();

    QString path = m_device->udi();

    QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE, path, UD_DBUS_INTERFACE_DISKS_DEVICE, "DriveEject");
    msg << (QStringList() << "unmount"); // unmount parameter
    return c.callWithCallback(msg, this, SLOT(slotDBusReply(QDBusMessage)), SLOT(slotDBusError(QDBusError)));
}

void UDisksOpticalDrive::slotDBusReply(const QDBusMessage &/*reply*/)
{
    m_ejectInProgress = false;
    m_device->broadcastActionDone("eject");
}

void UDisksOpticalDrive::slotDBusError(const QDBusError &error)
{
    m_ejectInProgress = false;
    m_device->broadcastActionDone("eject", m_device->errorToSolidError(error.name()),
                                  m_device->errorToString(error.name()) + ": " +error.message());
}

void UDisksOpticalDrive::slotEjectRequested()
{
    m_ejectInProgress = true;
    emit ejectRequested(m_device->udi());
}

void UDisksOpticalDrive::slotEjectDone(int error, const QString &errorString)
{
    m_ejectInProgress = false;
    emit ejectDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
}

void UDisksOpticalDrive::initReadWriteSpeeds() const
{
#if 0
    int read_speed, write_speed;
    char *write_speeds = 0;
    QByteArray device_file = QFile::encodeName(m_device->property("DeviceFile").toString());

    //qDebug("Doing open (\"%s\", O_RDONLY | O_NONBLOCK)", device_file.constData());
    int fd = open(device_file, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        qWarning("Cannot open %s: %s", device_file.constData(), strerror (errno));
        return;
    }

    if (get_read_write_speed(fd, &read_speed, &write_speed, &write_speeds) >= 0) {
        m_readSpeed = read_speed;
        m_writeSpeed = write_speed;

        QStringList list = QString::fromLatin1(write_speeds).split(',', QString::SkipEmptyParts);
        foreach (const QString & speed, list)
            m_writeSpeeds.append(speed.toInt());

        free(write_speeds);

        m_speedsInit = true;
    }

    close(fd);
#endif
}

QList<int> UDisksOpticalDrive::writeSpeeds() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    //qDebug() << "solid write speeds:" << m_writeSpeeds;
    return m_writeSpeeds;
}

int UDisksOpticalDrive::writeSpeed() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    return m_writeSpeed;
}

int UDisksOpticalDrive::readSpeed() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    return m_readSpeed;
}

Solid::OpticalDrive::MediumTypes UDisksOpticalDrive::supportedMedia() const
{
    const QStringList mediaTypes = m_device->prop("DriveMediaCompatibility").toStringList();
    Solid::OpticalDrive::MediumTypes supported;

    QMap<Solid::OpticalDrive::MediumType, QString> map;
    map[Solid::OpticalDrive::Cdr] = "optical_cd_r";
    map[Solid::OpticalDrive::Cdrw] = "optical_cd_rw";
    map[Solid::OpticalDrive::Dvd] = "optical_dvd";
    map[Solid::OpticalDrive::Dvdr] = "optical_dvd_r";
    map[Solid::OpticalDrive::Dvdrw] ="optical_dvd_rw";
    map[Solid::OpticalDrive::Dvdram] ="optical_dvd_ram";
    map[Solid::OpticalDrive::Dvdplusr] ="optical_dvd_plus_r";
    map[Solid::OpticalDrive::Dvdplusrw] ="optical_dvd_plus_rw";
    map[Solid::OpticalDrive::Dvdplusdl] ="optical_dvd_plus_r_dl";
    map[Solid::OpticalDrive::Dvdplusdlrw] ="optical_dvd_plus_rw_dl";
    map[Solid::OpticalDrive::Bd] ="optical_bd";
    map[Solid::OpticalDrive::Bdr] ="optical_bd_r";
    map[Solid::OpticalDrive::Bdre] ="optical_bd_re";
    map[Solid::OpticalDrive::HdDvd] ="optical_hddvd";
    map[Solid::OpticalDrive::HdDvdr] ="optical_hddvd_r";
    map[Solid::OpticalDrive::HdDvdrw] ="optical_hddvd_rw";
    // TODO add these to Solid
    //map[Solid::OpticalDrive::Mo] ="optical_mo";
    //map[Solid::OpticalDrive::Mr] ="optical_mrw";
    //map[Solid::OpticalDrive::Mrw] ="optical_mrw_w";

    foreach ( const Solid::OpticalDrive::MediumType & type, map.keys() )
    {
        if ( mediaTypes.contains( map[type] ) )
        {
            supported |= type;
        }
    }

    return supported;
}

void UDisksOpticalDrive::slotChanged()
{
    m_speedsInit = false; // reset the read/write speeds, changes eg. with an inserted media
}
