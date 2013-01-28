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

#include "wmistorageaccess.h"

#include <QDebug>
#include <QApplication>
#include <QWidget>

#include <unistd.h>

using namespace Solid::Backends::Wmi;

StorageAccess::StorageAccess(WmiDevice *device)
    : DeviceInterface(device), m_setupInProgress(false), m_teardownInProgress(false),
      m_passphraseRequested(false)
{
    connect(device, SIGNAL(propertyChanged(QMap<QString,int>)),
             this, SLOT(slotPropertyChanged(QMap<QString,int>)));
//    qDebug()<<"StorageAccess"<<m_device->type();

    m_logicalDisk = WmiDevice::win32LogicalDiskByDiskPartitionID(m_device->property("DeviceID").toString());
}

StorageAccess::~StorageAccess()
{

}


bool StorageAccess::isAccessible() const
{
    return !m_logicalDisk.isNull();
}

QString StorageAccess::filePath() const
{
    QString path = m_logicalDisk.getProperty("DeviceID").toString();
    if(!path.isNull())
        path.append("\\");
    return path;
}

bool Solid::Backends::Wmi::StorageAccess::isIgnored() const
{
    return m_logicalDisk.isNull();
}

bool StorageAccess::setup()
{
    if (m_teardownInProgress || m_setupInProgress) {
        return false;
    }
    m_setupInProgress = true;


    // if (m_device->property("info.interfaces").toStringList().contains("org.freedesktop.Wmi.Device.Volume.Crypto")) {
        // return requestPassphrase();
    // } else if (FstabHandling::isInFstab(m_device->property("block.device").toString())) {
        // return callSystemMount();
    // } else {
        // return callWmiVolumeMount();
    // }
    return false;
}

bool StorageAccess::teardown()
{
    if (m_teardownInProgress || m_setupInProgress) {
        return false;
    }
    m_teardownInProgress = true;

    // if (m_device->property("info.interfaces").toStringList().contains("org.freedesktop.Wmi.Device.Volume.Crypto")) {
        // return callCryptoTeardown();
    // } else if (FstabHandling::isInFstab(m_device->property("block.device").toString())) {
        // return callSystemUnmount();
    // } else {
        // return callWmiVolumeUnmount();
    // }
    return false;
}

void StorageAccess::slotPropertyChanged(const QMap<QString,int> &changes)
{
    if (changes.contains("volume.is_mounted"))
    {
        emit accessibilityChanged(isAccessible(), m_device->udi());
    }
}

void Solid::Backends::Wmi::StorageAccess::slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
/*
    Q_UNUSED(exitStatus);
    if (m_setupInProgress) {
        m_setupInProgress = false;

        if (exitCode==0) {
            emit setupDone(Solid::NoError, QVariant(), m_device->udi());
        } else {
            emit setupDone(Solid::UnauthorizedOperation,
                           m_process->readAllStandardError(),
                           m_device->udi());
        }
    } else if (m_teardownInProgress) {
        m_teardownInProgress = false;
        if (exitCode==0) {
            emit teardownDone(Solid::NoError, QVariant(), m_device->udi());
        } else {
            emit teardownDone(Solid::UnauthorizedOperation,
                              m_process->readAllStandardError(),
                              m_device->udi());
        }
    }

    delete m_process;
 */
}

QString generateReturnObjectPath()
{
    static int number = 1;

    return "/org/kde/solid/WmiStorageAccess_"+QString::number(number++);
}

bool StorageAccess::callWmiVolumeMount()
{
    // QDBusConnection c = QDBusConnection::systemBus();
    // QString udi = m_device->udi();
    // QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Wmi", udi,
                                                      // "org.freedesktop.Wmi.Device.Volume",
                                                      // "Mount");
    // QStringList options;
    // QStringList wmiOptions = m_device->property("volume.mount.valid_options").toStringList();

    // if (wmiOptions.contains("uid=")) {
        // options << "uid="+QString::number(::getuid());
    // }

    // msg << "" << "" << options;

    // return c.callWithCallback(msg, this,
                              // SLOT(slotDBusReply(QDBusMessage)),
                              // SLOT(slotDBusError(QDBusError)));
    return false;
}

bool StorageAccess::callWmiVolumeUnmount()
{
    // QDBusConnection c = QDBusConnection::systemBus();
    // QString udi = m_device->udi();
    // QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Wmi", udi,
                                                      // "org.freedesktop.Wmi.Device.Volume",
                                                      // "Unmount");

    // msg << QStringList();

    // return c.callWithCallback(msg, this,
                              // SLOT(slotDBusReply(QDBusMessage)),
                              // SLOT(slotDBusError(QDBusError)));
    return false;
}

bool Solid::Backends::Wmi::StorageAccess::callSystemMount()
{
/*
    const QString device = m_device->property("block.device").toString();
    m_process = FstabHandling::callSystemCommand("mount", device,
                                                 this, SLOT(slotProcessFinished(int,QProcess::ExitStatus)));

    return m_process!=0;
*/
    return 0;
}

bool Solid::Backends::Wmi::StorageAccess::callSystemUnmount()
{
/*
    const QString device = m_device->property("block.device").toString();
    m_process = FstabHandling::callSystemCommand("umount", device,
                                                 this, SLOT(slotProcessFinished(int,QProcess::ExitStatus)));

    return m_process!=0;
*/
    return 0;
}

//#include "backends/wmi/wmistorageaccess.moc"
