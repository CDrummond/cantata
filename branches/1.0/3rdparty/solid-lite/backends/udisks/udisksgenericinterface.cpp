/*
    Copyright 2009 Pino Toscano <pino@kde.org>

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

#include "udisksgenericinterface.h"

#include "udisksdevice.h"

using namespace Solid::Backends::UDisks;

GenericInterface::GenericInterface(UDisksDevice *device)
    : DeviceInterface(device)
{
/* TODO
    connect(device, SIGNAL(propertyChanged(QMap<QString,int>)),
            this, SIGNAL(propertyChanged(QMap<QString,int>)));
    connect(device, SIGNAL(conditionRaised(QString,QString)),
            this, SIGNAL(conditionRaised(QString,QString)));
*/
}

GenericInterface::~GenericInterface()
{
}

QVariant GenericInterface::property(const QString &key) const
{
    return m_device->prop(key);
}

QMap<QString, QVariant> GenericInterface::allProperties() const
{
    return m_device->allProperties();
}

bool GenericInterface::propertyExists(const QString &key) const
{
    return m_device->propertyExists(key);
}

//#include "backends/udisks/udisksgenericinterface.moc"
