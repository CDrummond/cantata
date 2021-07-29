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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <QFile>
#include <QDebug>

#include "udisksopticaldrive.h"
#include "udisks2.h"
#include "udisksdevice.h"
#include "dbus/manager.h"

using namespace Solid::Backends::UDisks2;

OpticalDrive::OpticalDrive(Device *device)
    : StorageDrive(device)
    , m_ejectInProgress(false)
    , m_readSpeed(0)
    , m_writeSpeed(0)
    , m_speedsInit(false)
{
    m_device->registerAction("eject", this,
                             SLOT(slotEjectRequested()),
                             SLOT(slotEjectDone(int, const QString&)));

    connect(m_device, SIGNAL(changed()), this, SLOT(slotChanged()));
}

OpticalDrive::~OpticalDrive()
{
}

bool OpticalDrive::eject()
{
    if (m_ejectInProgress)
        return false;
    m_ejectInProgress = true;
    m_device->broadcastActionRequested("eject");

    const QString path = m_device->udi();
    QDBusConnection c = QDBusConnection::connectToBus(QDBusConnection::SystemBus, "Solid::Udisks2::OpticalDrive::" + path);

    // if the device is mounted, unmount first
    QString blockPath;
    org::freedesktop::DBus::ObjectManager manager(UD2_DBUS_SERVICE, UD2_DBUS_PATH, c);
    QDBusPendingReply<DBUSManagerStruct> reply = manager.GetManagedObjects();
    reply.waitForFinished();
    if (!reply.isError()) {  // enum devices
        Q_FOREACH(const QDBusObjectPath &objPath, reply.value().keys()) {
            const QString udi = objPath.path();

            //qDebug() << "Inspecting" << udi;

            if (udi == UD2_DBUS_PATH_MANAGER || udi == UD2_UDI_DISKS_PREFIX || udi.startsWith(UD2_DBUS_PATH_JOBS))
                continue;

            Device device(udi);
            if (device.drivePath() == path && device.isMounted()) {
                //qDebug() << "Got mounted block device:" << udi;
                blockPath = udi;
                break;
            }
        }
    }
    else  // show error
    {
        qWarning() << "Failed enumerating UDisks2 objects:" << reply.error().name() << "\n" << reply.error().message();
    }

    if (!blockPath.isEmpty()) {
        //qDebug() << "Calling unmount on" << blockPath;
        QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, blockPath, UD2_DBUS_INTERFACE_FILESYSTEM, "Unmount");
        msg << QVariantMap();   // options, unused now
        c.call(msg, QDBus::BlockWithGui);
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, path, UD2_DBUS_INTERFACE_DRIVE, "Eject");
    msg << QVariantMap();
    return c.callWithCallback(msg, this, SLOT(slotDBusReply(const QDBusMessage &)), SLOT(slotDBusError(const QDBusError &)));
}

void OpticalDrive::slotDBusReply(const QDBusMessage &/*reply*/)
{
    m_ejectInProgress = false;
    m_device->broadcastActionDone("eject");
}

void OpticalDrive::slotDBusError(const QDBusError &error)
{
    m_ejectInProgress = false;
    m_device->broadcastActionDone("eject", m_device->errorToSolidError(error.name()),
                                  m_device->errorToString(error.name()) + ": " +error.message());
}

void OpticalDrive::slotEjectRequested()
{
    m_ejectInProgress = true;
    Q_EMIT ejectRequested(m_device->udi());
}

void OpticalDrive::slotEjectDone(int error, const QString &errorString)
{
    m_ejectInProgress = false;
    Q_EMIT ejectDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
}

void OpticalDrive::initReadWriteSpeeds() const
{
#if 0
    int read_speed, write_speed;
    char *write_speeds = 0;
    QByteArray device_file = QFile::encodeName(m_device->property("Device").toString());

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
        Q_FOREACH (const QString & speed, list)
            m_writeSpeeds.append(speed.toInt());

        free(write_speeds);

        m_speedsInit = true;
    }

    close(fd);
#endif
}

QList<int> OpticalDrive::writeSpeeds() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    //qDebug() << "solid write speeds:" << m_writeSpeeds;
    return m_writeSpeeds;
}

int OpticalDrive::writeSpeed() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    return m_writeSpeed;
}

int OpticalDrive::readSpeed() const
{
    if (!m_speedsInit)
        initReadWriteSpeeds();
    return m_readSpeed;
}

Solid::OpticalDrive::MediumTypes OpticalDrive::supportedMedia() const
{
    const QStringList mediaTypes = m_device->prop("MediaCompatibility").toStringList();
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

    Q_FOREACH ( const Solid::OpticalDrive::MediumType & type, map.keys() )
    {
        if ( mediaTypes.contains( map[type] ) )
        {
            supported |= type;
        }
    }

    return supported;
}

void OpticalDrive::slotChanged()
{
    m_speedsInit = false; // reset the read/write speeds, changes eg. with an inserted media
}
