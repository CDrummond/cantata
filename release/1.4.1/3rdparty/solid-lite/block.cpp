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

#include "block.h"
#include "block_p.h"

#include "soliddefs_p.h"
#include <solid-lite/ifaces/block.h>

Solid::Block::Block(QObject *backendObject)
    : DeviceInterface(*new BlockPrivate(), backendObject)
{
}

Solid::Block::~Block()
{

}

int Solid::Block::deviceMajor() const
{
    Q_D(const Block);
    return_SOLID_CALL(Ifaces::Block *, d->backendObject(), 0, deviceMajor());
}

int Solid::Block::deviceMinor() const
{
    Q_D(const Block);
    return_SOLID_CALL(Ifaces::Block *, d->backendObject(), 0, deviceMinor());
}

QString Solid::Block::device() const
{
    Q_D(const Block);
    return_SOLID_CALL(Ifaces::Block *, d->backendObject(), QString(), device());
}

//#include "block.moc"
