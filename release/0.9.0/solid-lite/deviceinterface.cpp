/*
    Copyright 2006-2007 Kevin Ottens <ervin@kde.org>

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

#include "deviceinterface.h"
#include "deviceinterface_p.h"

#include <solid-lite/ifaces/deviceinterface.h>

#include <QtCore/QMetaEnum>


Solid::DeviceInterface::DeviceInterface(DeviceInterfacePrivate &dd, QObject *backendObject)
    : d_ptr(&dd)
{
    Q_D(DeviceInterface);

    d->setBackendObject(backendObject);
}


Solid::DeviceInterface::~DeviceInterface()
{
    delete d_ptr;
    d_ptr = 0;
}

bool Solid::DeviceInterface::isValid() const
{
    Q_D(const DeviceInterface);
    return d->backendObject()!=0;
}

QString Solid::DeviceInterface::typeToString(Type type)
{
    int index = staticMetaObject.indexOfEnumerator("Type");
    QMetaEnum metaEnum = staticMetaObject.enumerator(index);
    return QString(metaEnum.valueToKey((int)type));
}

Solid::DeviceInterface::Type Solid::DeviceInterface::stringToType(const QString &type)
{
    int index = staticMetaObject.indexOfEnumerator("Type");
    QMetaEnum metaEnum = staticMetaObject.enumerator(index);
    return (Type)metaEnum.keyToValue(type.toUtf8());
}

QString Solid::DeviceInterface::typeDescription(Type type)
{
    switch (type)
    {
    case Unknown:
        return QObject::tr("Unknown", "Unknown device type");
    case GenericInterface:
        return QObject::tr("Generic Interface", "Generic Interface device type");
    //case Processor:
    //    return QObject::tr("Processor", "Processor device type");
    case Block:
        return QObject::tr("Block", "Block device type");
    case StorageAccess:
        return QObject::tr("Storage Access", "Storage Access device type");
    case StorageDrive:
        return QObject::tr("Storage Drive", "Storage Drive device type");
    case OpticalDrive:
        return QObject::tr("Optical Drive", "Optical Drive device type");
    case StorageVolume:
        return QObject::tr("Storage Volume", "Storage Volume device type");
    case OpticalDisc:
        return QObject::tr("Optical Disc", "Optical Disc device type");
    //case Camera:
    //    return QObject::tr("Camera", "Camera device type");
    case PortableMediaPlayer:
        return QObject::tr("Portable Media Player", "Portable Media Player device type");
    /*case NetworkInterface:
        return QObject::tr("Network Interface", "Network Interface device type");
    case AcAdapter:
        return QObject::tr("Ac Adapter", "Ac Adapter device type");
    case Battery:
        return QObject::tr("Battery", "Battery device type");
    case Button:
        return QObject::tr("Button", "Button device type");
    case AudioInterface:
        return QObject::tr("Audio Interface", "Audio Interface device type");
    case DvbInterface:
        return QObject::tr("Dvb Interface", "Dvb Interface device type");
    case Video:
        return QObject::tr("Video", "Video device type");
    case SerialInterface:
        return QObject::tr("Serial Interface", "Serial Interface device type");
    case SmartCardReader:
        return QObject::tr("Smart Card Reader", "Smart Card Reader device type");
    case InternetGateway:
        return QObject::tr("Internet Gateway Device", "Internet Gateway device type");
    case NetworkShare:
        return QObject::tr("Network Share", "Network Share device type");
    */
    case Last:
        return QString();
    }
    return QString();
}

Solid::DeviceInterfacePrivate::DeviceInterfacePrivate()
    : m_devicePrivate(0)
{

}

Solid::DeviceInterfacePrivate::~DeviceInterfacePrivate()
{

}

QObject *Solid::DeviceInterfacePrivate::backendObject() const
{
    return m_backendObject.data();
}

void Solid::DeviceInterfacePrivate::setBackendObject(QObject *object)
{
    m_backendObject = object;
}

Solid::DevicePrivate* Solid::DeviceInterfacePrivate::devicePrivate() const
{
    return m_devicePrivate;
}

void Solid::DeviceInterfacePrivate::setDevicePrivate(DevicePrivate *devicePrivate)
{
    m_devicePrivate = devicePrivate;
}

//#include "deviceinterface.moc"
