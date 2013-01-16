/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010 Lukas Tinkl <ltinkl@redhat.com>

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

#include "udisksmanager.h"
#include "udisks.h"

#include <QtDBus/QDBusReply>
#include <QtCore/QDebug>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusConnectionInterface>

#include "../shared/rootdevice.h"

using namespace Solid::Backends::UDisks;
using namespace Solid::Backends::Shared;

UDisksManager::UDisksManager(QObject *parent)
    : Solid::Ifaces::DeviceManager(parent),
      m_manager(UD_DBUS_SERVICE,
                UD_DBUS_PATH,
                UD_DBUS_INTERFACE_DISKS,
                QDBusConnection::systemBus())
{
    m_supportedInterfaces
            << Solid::DeviceInterface::GenericInterface
            << Solid::DeviceInterface::Block
            << Solid::DeviceInterface::StorageAccess
            << Solid::DeviceInterface::StorageDrive
            << Solid::DeviceInterface::OpticalDrive
            << Solid::DeviceInterface::OpticalDisc
            << Solid::DeviceInterface::StorageVolume;

    qDBusRegisterMetaType<QList<QDBusObjectPath> >();
    qDBusRegisterMetaType<QVariantMap>();

    bool serviceFound = m_manager.isValid();
    if (!serviceFound) {
        // find out whether it will be activated automatically
        QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.DBus",
                                                              "/org/freedesktop/DBus",
                                                              "org.freedesktop.DBus",
                                                              "ListActivatableNames");

        QDBusReply<QStringList> reply = QDBusConnection::systemBus().call(message);
        if (reply.isValid() && reply.value().contains(UD_DBUS_SERVICE)) {
            QDBusConnection::systemBus().interface()->startService(UD_DBUS_SERVICE);
            serviceFound = true;
        }
    }

    if (serviceFound) {
        connect(&m_manager, SIGNAL(DeviceAdded(QDBusObjectPath)),
                this, SLOT(slotDeviceAdded(QDBusObjectPath)));
        connect(&m_manager, SIGNAL(DeviceRemoved(QDBusObjectPath)),
                this, SLOT(slotDeviceRemoved(QDBusObjectPath)));
        connect(&m_manager, SIGNAL(DeviceChanged(QDBusObjectPath)),
                this, SLOT(slotDeviceChanged(QDBusObjectPath)));
    }
}

UDisksManager::~UDisksManager()
{

}

QObject* UDisksManager::createDevice(const QString& udi)
{
    if (udi==udiPrefix()) {
        RootDevice *root = new RootDevice(udi);

        root->setProduct(tr("Storage"));
        root->setDescription(tr("Storage devices"));
        root->setIcon("server-database"); // Obviously wasn't meant for that, but maps nicely in oxygen icon set :-p

        return root;

    } else if (deviceCache().contains(udi)) {
        return new UDisksDevice(udi);

    } else {
        return 0;
    }
}

QStringList UDisksManager::devicesFromQuery(const QString& parentUdi, Solid::DeviceInterface::Type type)
{
    QStringList result;

    if (!parentUdi.isEmpty())
    {
        foreach (const QString &udi, deviceCache())
        {
            if (udi==udiPrefix())
                continue;

            UDisksDevice device(udi);
            if (device.queryDeviceInterface(type) && device.parentUdi() == parentUdi)
                result << udi;
        }

        return result;
    }
    else if (type != Solid::DeviceInterface::Unknown)
    {
        foreach (const QString &udi, deviceCache())
        {
            if (udi==udiPrefix())
                continue;

            UDisksDevice device(udi);
            if (device.queryDeviceInterface(type))
                result << udi;
        }

        return result;
    }

    return deviceCache();
}

QStringList UDisksManager::allDevices()
{
    m_knownDrivesWithMedia.clear();
    m_deviceCache.clear();
    m_deviceCache << udiPrefix();

    foreach(const QString &udi, allDevicesInternal())
    {
        m_deviceCache.append(udi);

        UDisksDevice device(udi);
        if (device.queryDeviceInterface(Solid::DeviceInterface::OpticalDrive)) // forge a special (separate) device for optical discs
        {
            if (device.prop("DeviceIsOpticalDisc").toBool())
            {
                if (!m_knownDrivesWithMedia.contains(udi))
                    m_knownDrivesWithMedia.append(udi);
                m_deviceCache.append(udi + ":media");
            }
        }
    }

    return m_deviceCache;
}

QStringList UDisksManager::allDevicesInternal()
{
    QDBusReply<QList<QDBusObjectPath> > reply = m_manager.call("EnumerateDevices");

    if (!reply.isValid()) {
        qWarning() << Q_FUNC_INFO << " error: " << reply.error().name();
        return QStringList();
    }

    QStringList retList;
    foreach(const QDBusObjectPath &path, reply.value()) {
        retList << path.path();
    }

    return retList;
}

QSet< Solid::DeviceInterface::Type > UDisksManager::supportedInterfaces() const
{
    return m_supportedInterfaces;
}

QString UDisksManager::udiPrefix() const
{
    return UD_UDI_DISKS_PREFIX;
}

void UDisksManager::slotDeviceAdded(const QDBusObjectPath &opath)
{
    const QString udi = opath.path();

    if (!m_deviceCache.contains(udi)) {
        m_deviceCache.append(udi);
    }

    UDisksDevice device(udi);
    if (device.queryDeviceInterface(Solid::DeviceInterface::StorageDrive)
            && !device.prop("DeviceIsMediaAvailable").toBool()
            && !m_dirtyDevices.contains(udi))
        m_dirtyDevices.append(udi);

    emit deviceAdded(udi);
    slotDeviceChanged(opath);  // case: hotswap event (optical drive with media inside)
}

void UDisksManager::slotDeviceRemoved(const QDBusObjectPath &opath)
{
    const QString udi = opath.path();

    // case: hotswap event (optical drive with media inside)
    if (m_knownDrivesWithMedia.contains(udi)) {
        m_knownDrivesWithMedia.removeAll(udi);
        m_deviceCache.removeAll(udi + ":media");
        emit deviceRemoved(udi + ":media");
    }

    if (m_dirtyDevices.contains(udi))
        m_dirtyDevices.removeAll(udi);

    emit deviceRemoved(udi);
    m_deviceCache.removeAll(opath.path());
}

void UDisksManager::slotDeviceChanged(const QDBusObjectPath &opath)
{
    const QString udi = opath.path();
    UDisksDevice device(udi);

    if (device.queryDeviceInterface(Solid::DeviceInterface::OpticalDrive))
    {
        if (!m_knownDrivesWithMedia.contains(udi) && device.prop("DeviceIsOpticalDisc").toBool())
        {
            m_knownDrivesWithMedia.append(udi);
            if (!m_deviceCache.isEmpty()) {
                m_deviceCache.append(udi + ":media");
            }
            emit deviceAdded(udi + ":media");
        }

        if (m_knownDrivesWithMedia.contains(udi) && !device.prop("DeviceIsOpticalDisc").toBool())
        {
            m_knownDrivesWithMedia.removeAll(udi);
            m_deviceCache.removeAll(udi + ":media");
            emit deviceRemoved(udi + ":media");
        }
    }

    if (device.queryDeviceInterface(Solid::DeviceInterface::StorageDrive)
            && device.prop("DeviceIsMediaAvailable").toBool()
            && m_dirtyDevices.contains(udi))
    {
        //qDebug() << "dirty device added:" << udi;
        emit deviceAdded(udi);
        m_dirtyDevices.removeAll(udi);
    }
}

const QStringList &UDisksManager::deviceCache()
{
    if (m_deviceCache.isEmpty())
        allDevices();

    return m_deviceCache;
}


//#include "backends/udisks/udisksmanager.moc"
