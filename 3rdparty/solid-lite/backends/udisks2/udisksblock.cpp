/*
    Copyright 2012 Lukáš Tinkl <ltinkl@redhat.com>

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

#include <linux/kdev_t.h>

#include <QFile>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDomDocument>

#include "udisksblock.h"

using namespace Solid::Backends::UDisks2;

Block::Block(Device *dev)
    : DeviceInterface(dev)
{
    m_devNum = m_device->prop("DeviceNumber").toULongLong();
    m_devFile = QFile::decodeName(m_device->prop("Device").toByteArray());

    // we have a drive (non-block device for udisks), so let's find the corresponding (real) block device
    if (m_devNum == 0 || m_devFile.isEmpty()) {
        const QString path = "/org/freedesktop/UDisks2/block_devices";
        QDBusMessage call = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, path,
                                                           DBUS_INTERFACE_INTROSPECT, "Introspect");
        QDBusPendingReply<QString> reply = QDBusConnection::systemBus().asyncCall(call);
        reply.waitForFinished();

        if (reply.isValid()) {
            QDomDocument dom;
            dom.setContent(reply.value());
            QDomNodeList nodeList = dom.documentElement().elementsByTagName("node");
            for (int i = 0; i < nodeList.count(); i++) {
                QDomElement nodeElem = nodeList.item(i).toElement();
                if (!nodeElem.isNull() && nodeElem.hasAttribute("name")) {
                    const QString udi = path + "/" + nodeElem.attribute("name");

                    Device device(udi);
                    if (device.drivePath() == dev->udi()) {
                        m_devNum = device.prop("DeviceNumber").toULongLong();
                        m_devFile = QFile::decodeName(device.prop("Device").toByteArray());
                        break;
                    }
                }
            }
        }
        else
            qWarning() << "Failed enumerating UDisks2 objects:" << reply.error().name() << "\n" << reply.error().message();
    }

    //qDebug() << "devnum:" << m_devNum << "dev file:" << m_devFile;
}

Block::~Block()
{
}

QString Block::device() const
{
    return m_devFile;
}

int Block::deviceMinor() const
{
    return MINOR(m_devNum);
}

int Block::deviceMajor() const
{
    return MAJOR(m_devNum);
}
