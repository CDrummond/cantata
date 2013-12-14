/*
    Copyright 2010 Rafael Fernández López <ereslibre@kde.org>

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

#include "udevmanager.h"

#include "udev.h"
#include "udevdevice.h"
#include "../shared/rootdevice.h"

#include <QSet>
#include <QFile>
#include <QDebug>

#define DETAILED_OUTPUT 0

using namespace Solid::Backends::UDev;
using namespace Solid::Backends::Shared;

class UDevManager::Private
{
public:
    Private();
    ~Private();

    bool isOfInterest(const UdevQt::Device &device);

    UdevQt::Client *m_client;
    QSet<Solid::DeviceInterface::Type> m_supportedInterfaces;
};

UDevManager::Private::Private()
{
    QStringList subsystems;
    subsystems << "processor";
    subsystems << "sound";
    subsystems << "tty";
    subsystems << "dvb";
    subsystems << "video4linux";
    subsystems << "net";
    subsystems << "usb";
    m_client = new UdevQt::Client(subsystems);
}

UDevManager::Private::~Private()
{
    delete m_client;
}

bool UDevManager::Private::isOfInterest(const UdevQt::Device &device)
{
#if DETAILED_OUTPUT
    qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";
    qDebug() << "Path:" << device.sysfsPath();
    qDebug() << "Properties:" << device.deviceProperties();
    Q_FOREACH (const QString &key, device.deviceProperties()) {
        qDebug() << "\t" << key << ":" << device.deviceProperty(key).toString();
    }
    qDebug() << "Driver:" << device.driver();
    qDebug() << "Subsystem:" << device.subsystem();
    qDebug() << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
#endif
    if (device.driver() == QLatin1String("processor")) {
        // Linux ACPI reports processor slots, rather than processors.
        // Empty slots will not have a system device associated with them.
        return QFile::exists(device.sysfsPath() + "/sysdev");
    }
    if (device.subsystem() == QLatin1String("sound") &&
            device.deviceProperty("SOUND_FORM_FACTOR").toString() != "internal") {
        return true;
    }

    if (device.subsystem() == QLatin1String("tty")) {
        QString path = device.deviceProperty("DEVPATH").toString();

        int lastSlash = path.length() - path.lastIndexOf(QLatin1String("/")) -1;
        #if QT_VERSION < 0x050000
        QByteArray lastElement = path.right(lastSlash).toAscii();
        #else
        QByteArray lastElement = path.right(lastSlash).toLatin1();
        #endif

        if (lastElement.startsWith("tty") && !path.startsWith("/devices/virtual")) {
            return true;
        }
    }
    return device.subsystem() == QLatin1String("dvb") ||
           device.subsystem() == QLatin1String("video4linux") ||
           device.subsystem() == QLatin1String("net") ||
           device.deviceProperty("ID_MEDIA_PLAYER").toString().isEmpty() == false || // media-player-info recognized devices
           device.deviceProperty("ID_GPHOTO2").toInt() == 1; // GPhoto2 cameras
}

UDevManager::UDevManager(QObject *parent)
    : Solid::Ifaces::DeviceManager(parent),
      d(new Private)
{
    connect(d->m_client, SIGNAL(deviceAdded(UdevQt::Device)), this, SLOT(slotDeviceAdded(UdevQt::Device)));
    connect(d->m_client, SIGNAL(deviceRemoved(UdevQt::Device)), this, SLOT(slotDeviceRemoved(UdevQt::Device)));

    d->m_supportedInterfaces << Solid::DeviceInterface::GenericInterface
                             /*<< Solid::DeviceInterface::Processor
                             << Solid::DeviceInterface::AudioInterface
                             << Solid::DeviceInterface::NetworkInterface
                             << Solid::DeviceInterface::SerialInterface
                             << Solid::DeviceInterface::Camera */
                             << Solid::DeviceInterface::PortableMediaPlayer
                             //<< Solid::DeviceInterface::DvbInterface
                             << Solid::DeviceInterface::Block
                             /*<< Solid::DeviceInterface::Video*/;
}

UDevManager::~UDevManager()
{
    delete d;
}

QString UDevManager::udiPrefix() const
{
    return QString::fromLatin1(UDEV_UDI_PREFIX);
}

QSet<Solid::DeviceInterface::Type> UDevManager::supportedInterfaces() const
{
    return d->m_supportedInterfaces;
}

QStringList UDevManager::allDevices()
{
    QStringList res;
    const UdevQt::DeviceList deviceList = d->m_client->allDevices();
    foreach (const UdevQt::Device &device, deviceList) {
        if (d->isOfInterest(device)) {
            res << udiPrefix() + device.sysfsPath();
        }
    }
    return res;
}

QStringList UDevManager::devicesFromQuery(const QString &parentUdi,
                                          Solid::DeviceInterface::Type type)
{
    QStringList allDev = allDevices();
    QStringList result;

    if (!parentUdi.isEmpty()) {
        foreach (const QString &udi, allDev) {
            UDevDevice device(d->m_client->deviceBySysfsPath(udi.right(udi.size() - udiPrefix().size())));
            if (device.queryDeviceInterface(type) && device.parentUdi() == parentUdi) {
                result << udi;
            }
        }

        return result;
    } else if (type != Solid::DeviceInterface::Unknown) {
        foreach (const QString &udi, allDev) {
            UDevDevice device(d->m_client->deviceBySysfsPath(udi.right(udi.size() - udiPrefix().size())));
            if (device.queryDeviceInterface(type)) {
                result << udi;
            }
        }

        return result;
    } else {
        return allDev;
    }
}

QObject *UDevManager::createDevice(const QString &udi_)
{
    if (udi_ == udiPrefix()) {
        RootDevice *const device = new RootDevice(UDEV_UDI_PREFIX);
        device->setProduct(tr("Devices"));
        device->setDescription(tr("Devices declared in your system"));
        device->setIcon("computer");
        return device;
    }
    const QString udi = udi_.right(udi_.size() - udiPrefix().size());
    UdevQt::Device device = d->m_client->deviceBySysfsPath(udi);
    if (d->isOfInterest(device) || QFile::exists(udi)) {
        return new UDevDevice(device);
    }
    return 0;
}

void UDevManager::slotDeviceAdded(const UdevQt::Device &device)
{
    if (d->isOfInterest(device)) {
        emit deviceAdded(udiPrefix() + device.sysfsPath());
    }
}

void UDevManager::slotDeviceRemoved(const UdevQt::Device &device)
{
    if (d->isOfInterest(device)) {
        emit deviceRemoved(udiPrefix() + device.sysfsPath());
    }
}
