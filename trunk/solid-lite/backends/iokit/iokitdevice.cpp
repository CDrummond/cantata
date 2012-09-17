/*
    Copyright 2009 Harald Fernengel <harry@kdevelop.org>

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


#include "iokitdevice.h"
#include "iokitgenericinterface.h"
//#include "iokitprocessor.h"
//#include "iokitbattery.h"
//#include "iokitnetworkinterface.h"
//#include "iokitserialinterface.h"

#include <QtCore/qdebug.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
//#include <IOKit/network/IOEthernetInterface.h>

#include <CoreFoundation/CoreFoundation.h>

// from cfhelper.cpp
extern QMap<QString, QVariant> q_toVariantMap(const CFMutableDictionaryRef &dict);

namespace Solid { namespace Backends { namespace IOKit {

// returns a solid type from an entry and its properties
static Solid::DeviceInterface::Type typeFromEntry(const io_registry_entry_t &entry,
        const QMap<QString, QVariant> &properties)
{
    /*
    if (IOObjectConformsTo(entry, kIOEthernetInterfaceClass))
        return Solid::DeviceInterface::NetworkInterface;
    if (IOObjectConformsTo(entry, "AppleACPICPU"))
        return Solid::DeviceInterface::Processor;
    if (IOObjectConformsTo(entry, "IOSerialBSDClient"))
        return Solid::DeviceInterface::SerialInterface;
    if (IOObjectConformsTo(entry, "AppleSmartBattery"))
        return Solid::DeviceInterface::Battery;
    */
    return Solid::DeviceInterface::Unknown;
}

// gets all properties from an entry into a QMap
static QMap<QString, QVariant> getProperties(const io_registry_entry_t &entry)
{
    CFMutableDictionaryRef propertyDict = 0;

    if (IORegistryEntryCreateCFProperties(entry, &propertyDict, kCFAllocatorDefault, kNilOptions) != KERN_SUCCESS) {
        return QMap<QString, QVariant>();
    }

    QMap<QString, QVariant> result = q_toVariantMap(propertyDict);

    CFRelease(propertyDict);

    return result;
}

// gets the parent's Udi from an entry
static QString getParentDeviceUdi(const io_registry_entry_t &entry)
{
    io_registry_entry_t parent = 0;
    kern_return_t ret = IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent);
    if (ret != KERN_SUCCESS) {
        // don't release parent here - docs say only on success
        return QString();
    }

    QString result;
    io_string_t pathName;
    ret = IORegistryEntryGetPath(parent, kIOServicePlane, pathName);
    if (ret == KERN_SUCCESS)
        result = QString::fromUtf8(pathName);

    // now we can release the parent
    IOObjectRelease(parent);

    return result;
}


class IOKitDevicePrivate
{
public:
    inline IOKitDevicePrivate()
        : type(Solid::DeviceInterface::Unknown)
    {}

    void init(const QString &udiString, const io_registry_entry_t & entry);

    QString udi;
    QString parentUdi;
    QMap<QString, QVariant> properties;
    Solid::DeviceInterface::Type type;
};

void IOKitDevicePrivate::init(const QString &udiString, const io_registry_entry_t &entry)
{
    Q_ASSERT(entry != MACH_PORT_NULL);

    udi = udiString;

    properties = getProperties(entry);

    io_name_t className;
    IOObjectGetClass(entry, className);
    properties["className"] = QString::fromUtf8(className);

    parentUdi = getParentDeviceUdi(entry);
    type = typeFromEntry(entry, properties);

    IOObjectRelease(entry);
}

IOKitDevice::IOKitDevice(const QString &udi, const io_registry_entry_t &entry)
    : d(new IOKitDevicePrivate)
{
    d->init(udi, entry);
}

IOKitDevice::IOKitDevice(const QString &udi)
    : d(new IOKitDevicePrivate)
{
    io_registry_entry_t entry = IORegistryEntryFromPath(
            kIOMasterPortDefault,
            udi.toLocal8Bit().constData());

    if (entry == MACH_PORT_NULL) {
        qDebug() << Q_FUNC_INFO << "Tried to create Device from invalid UDI" << udi;
        return;
    }

    d->init(udi, entry);
}

IOKitDevice::~IOKitDevice()
{
    delete d;
}

QString IOKitDevice::udi() const
{
    return d->udi;
}

QString IOKitDevice::parentUdi() const
{
    return d->parentUdi;
}

QString IOKitDevice::vendor() const
{
    return QString(); // TODO
}

QString IOKitDevice::product() const
{
    return QString(); // TODO
}

QString IOKitDevice::icon() const
{
    return QString(); // TODO
}

QStringList IOKitDevice::emblems() const
{
    return QStringList(); // TODO
}

QString IOKitDevice::description() const
{
    return product(); // TODO
}

QVariant IOKitDevice::property(const QString &key) const
{
    return d->properties.value(key);
}

QMap<QString, QVariant> IOKitDevice::allProperties() const
{
    return d->properties;
}

bool IOKitDevice::propertyExists(const QString &key) const
{
    return d->properties.contains(key);
}

bool IOKitDevice::queryDeviceInterface(const Solid::DeviceInterface::Type &type) const
{
    return (type == Solid::DeviceInterface::GenericInterface
            || type == d->type);
}

QObject *IOKitDevice::createDeviceInterface(const Solid::DeviceInterface::Type &type)
{
    QObject *iface = 0;

    switch (type)
    {
    case Solid::DeviceInterface::GenericInterface:
        iface = new GenericInterface(this);
        break;
    /*
    case Solid::DeviceInterface::Processor:
        if (d->type == Solid::DeviceInterface::Processor)
            iface = new Processor(this);
        break;
    case Solid::DeviceInterface::NetworkInterface:
        if (d->type == Solid::DeviceInterface::NetworkInterface)
            iface = new NetworkInterface(this);
        break;
    case Solid::DeviceInterface::SerialInterface:
        if (d->type == Solid::DeviceInterface::SerialInterface)
            iface = new SerialInterface(this);
        break;
    case Solid::DeviceInterface::Battery:
        if (d->type == Solid::DeviceInterface::Battery)
            iface = new Battery(this);
        break;
    */
    // the rest is TODO
    }


    return iface;
}


} } } // namespaces

