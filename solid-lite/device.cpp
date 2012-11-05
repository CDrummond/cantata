/*
    Copyright 2005-2007 Kevin Ottens <ervin@kde.org>

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

#include "device.h"
#include "device_p.h"
#include "devicenotifier.h"
#include "devicemanager_p.h"

#include "deviceinterface_p.h"
#include "soliddefs_p.h"

#include <solid-lite/ifaces/device.h>

#include <solid-lite/genericinterface.h>
#include <solid-lite/ifaces/genericinterface.h>
//#include <solid-lite/processor.h>
//#include <solid-lite/ifaces/processor.h>
#include <solid-lite/block.h>
#include <solid-lite/ifaces/block.h>
#include <solid-lite/storageaccess.h>
#include <solid-lite/ifaces/storageaccess.h>
#include <solid-lite/storagedrive.h>
#include <solid-lite/ifaces/storagedrive.h>
//#include <solid-lite/opticaldrive.h>
//#include <solid-lite/ifaces/opticaldrive.h>
#include <solid-lite/storagevolume.h>
#include <solid-lite/ifaces/storagevolume.h>
//#include <solid-lite/opticaldisc.h>
//#include <solid-lite/ifaces/opticaldisc.h>
//#include <solid-lite/camera.h>
//#include <solid-lite/ifaces/camera.h>
#include <solid-lite/portablemediaplayer.h>
#include <solid-lite/ifaces/portablemediaplayer.h>
//#include <solid-lite/networkinterface.h>
//#include <solid-lite/ifaces/networkinterface.h>
//#include <solid-lite/networkshare.h>
//#include <solid-lite/ifaces/networkshare.h>
//#include <solid-lite/acadapter.h>
//#include <solid-lite/ifaces/acadapter.h>
//#include <solid-lite/battery.h>
//#include <solid-lite/ifaces/battery.h>
//#include <solid-lite/button.h>
//#include <solid-lite/ifaces/button.h>
//#include <solid-lite/audiointerface.h>
//#include <solid-lite/ifaces/audiointerface.h>
//#include <solid-lite/dvbinterface.h>
//#include <solid-lite/ifaces/dvbinterface.h>
//#include <solid-lite/video.h>
//#include <solid-lite/ifaces/video.h>
//#include <solid-lite/serialinterface.h>
//#include <solid-lite/ifaces/serialinterface.h>
//#include <solid-lite/smartcardreader.h>
//#include <solid-lite/ifaces/smartcardreader.h>
//#include <solid-lite/internetgateway.h>
//#include <solid-lite/ifaces/internetgateway.h>


Solid::Device::Device(const QString &udi)
{
    DeviceManagerPrivate *manager
        = static_cast<DeviceManagerPrivate *>(Solid::DeviceNotifier::instance());
    d = manager->findRegisteredDevice(udi);
}

Solid::Device::Device(const Device &device)
    : d(device.d)
{
}

Solid::Device::~Device()
{
}

Solid::Device &Solid::Device::operator=(const Solid::Device &device)
{
    d = device.d;
    return *this;
}

bool Solid::Device::isValid() const
{
    return d->backendObject()!=0;
}

QString Solid::Device::udi() const
{
    return d->udi();
}

QString Solid::Device::parentUdi() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QString(), parentUdi());
}

Solid::Device Solid::Device::parent() const
{
    QString udi = parentUdi();

    if (udi.isEmpty())
    {
        return Device();
    }
    else
    {
        return Device(udi);
    }
}

QString Solid::Device::vendor() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QString(), vendor());
}

QString Solid::Device::product() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QString(), product());
}

QString Solid::Device::icon() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QString(), icon());
}

QStringList Solid::Device::emblems() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QStringList(), emblems());
}

QString Solid::Device::description() const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), QString(), description());
}

bool Solid::Device::isDeviceInterface(const DeviceInterface::Type &type) const
{
    return_SOLID_CALL(Ifaces::Device *, d->backendObject(), false, queryDeviceInterface(type));
}

#define deviceinterface_cast(IfaceType, DevType, backendObject) \
    (qobject_cast<IfaceType *>(backendObject) ? new DevType(backendObject) : 0)

Solid::DeviceInterface *Solid::Device::asDeviceInterface(const DeviceInterface::Type &type)
{
    const Solid::DeviceInterface *interface = const_cast<const Device *>(this)->asDeviceInterface(type);
    return const_cast<Solid::DeviceInterface *>(interface);
}

const Solid::DeviceInterface *Solid::Device::asDeviceInterface(const DeviceInterface::Type &type) const
{
    Ifaces::Device *device = qobject_cast<Ifaces::Device *>(d->backendObject());

    if (device!=0)
    {
        DeviceInterface *iface = d->interface(type);

        if (iface!=0) {
            return iface;
        }

        QObject *dev_iface = device->createDeviceInterface(type);

        if (dev_iface!=0)
        {
            switch (type)
            {
            case DeviceInterface::GenericInterface:
                iface = deviceinterface_cast(Ifaces::GenericInterface, GenericInterface, dev_iface);
                break;
            //case DeviceInterface::Processor:
            //    iface = deviceinterface_cast(Ifaces::Processor, Processor, dev_iface);
            //    break;
            case DeviceInterface::Block:
                iface = deviceinterface_cast(Ifaces::Block, Block, dev_iface);
                break;
            case DeviceInterface::StorageAccess:
                iface = deviceinterface_cast(Ifaces::StorageAccess, StorageAccess, dev_iface);
                break;
            case DeviceInterface::StorageDrive:
                iface = deviceinterface_cast(Ifaces::StorageDrive, StorageDrive, dev_iface);
                break;
            //case DeviceInterface::OpticalDrive:
            //    iface = deviceinterface_cast(Ifaces::OpticalDrive, OpticalDrive, dev_iface);
            //    break;
            case DeviceInterface::StorageVolume:
                iface = deviceinterface_cast(Ifaces::StorageVolume, StorageVolume, dev_iface);
                break;
            //case DeviceInterface::OpticalDisc:
            //    iface = deviceinterface_cast(Ifaces::OpticalDisc, OpticalDisc, dev_iface);
            //    break;
            //case DeviceInterface::Camera:
            //    iface = deviceinterface_cast(Ifaces::Camera, Camera, dev_iface);
            //    break;
            case DeviceInterface::PortableMediaPlayer:
                iface = deviceinterface_cast(Ifaces::PortableMediaPlayer, PortableMediaPlayer, dev_iface);
                break;
            /*case DeviceInterface::NetworkInterface:
                iface = deviceinterface_cast(Ifaces::NetworkInterface, NetworkInterface, dev_iface);
                break;
            case DeviceInterface::AcAdapter:
                iface = deviceinterface_cast(Ifaces::AcAdapter, AcAdapter, dev_iface);
                break;
            case DeviceInterface::Battery:
                iface = deviceinterface_cast(Ifaces::Battery, Battery, dev_iface);
                break;
            case DeviceInterface::Button:
                iface = deviceinterface_cast(Ifaces::Button, Button, dev_iface);
                break;
            case DeviceInterface::AudioInterface:
                iface = deviceinterface_cast(Ifaces::AudioInterface, AudioInterface, dev_iface);
                break;
            case DeviceInterface::DvbInterface:
                iface = deviceinterface_cast(Ifaces::DvbInterface, DvbInterface, dev_iface);
                break;
            case DeviceInterface::Video:
                iface = deviceinterface_cast(Ifaces::Video, Video, dev_iface);
                break;
            case DeviceInterface::SerialInterface:
                iface = deviceinterface_cast(Ifaces::SerialInterface, SerialInterface, dev_iface);
                break;
            case DeviceInterface::SmartCardReader:
                iface = deviceinterface_cast(Ifaces::SmartCardReader, SmartCardReader, dev_iface);
                break;
            case DeviceInterface::InternetGateway:
                iface = deviceinterface_cast(Ifaces::InternetGateway, InternetGateway, dev_iface);
                break;
            case DeviceInterface::NetworkShare:
                iface = deviceinterface_cast(Ifaces::NetworkShare, NetworkShare, dev_iface);
                break;
            */
            case DeviceInterface::Unknown:
            case DeviceInterface::Last:
                break;
            }
        }

        if (iface!=0)
        {
            // Lie on the constness since we're simply doing caching here
            const_cast<Device *>(this)->d->setInterface(type, iface);
            iface->d_ptr->setDevicePrivate(d.data());
        }

        return iface;
    }
    else
    {
        return 0;
    }
}


//////////////////////////////////////////////////////////////////////


Solid::DevicePrivate::DevicePrivate(const QString &udi)
    : QObject(), QSharedData(), m_udi(udi)
{
}

Solid::DevicePrivate::~DevicePrivate()
{
    foreach (DeviceInterface *iface, m_ifaces) {
        delete iface->d_ptr->backendObject();
    }
    setBackendObject(0);
}

void Solid::DevicePrivate::_k_destroyed(QObject *object)
{
    Q_UNUSED(object);
    setBackendObject(0);
}

void Solid::DevicePrivate::setBackendObject(Ifaces::Device *object)
{

    if (m_backendObject) {
        m_backendObject.data()->disconnect(this);
    }

    delete m_backendObject.data();
    m_backendObject = object;

    if (object) {
        connect(object, SIGNAL(destroyed(QObject*)),
                this, SLOT(_k_destroyed(QObject*)));
    }

    if (!m_ifaces.isEmpty()) {
        foreach (DeviceInterface *iface, m_ifaces) {
            delete iface;
        }

        m_ifaces.clear();
        if (!ref.deref()) deleteLater();
    }
}

Solid::DeviceInterface *Solid::DevicePrivate::interface(const DeviceInterface::Type &type) const
{
    return m_ifaces[type];
}

void Solid::DevicePrivate::setInterface(const DeviceInterface::Type &type, DeviceInterface *interface)
{
    if(m_ifaces.isEmpty())
        ref.ref();
    m_ifaces[type] = interface;
}

// #include "device_p.moc"
