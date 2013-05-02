/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010-2012 Lukáš Tinkl <ltinkl@redhat.com>
    Copyright 2012 Dan Vrátil <dvratil@redhat.com>

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

#include "udisksdevicebackend.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDomDocument>

#include "solid-lite/deviceinterface.h"
#include "solid-lite/genericinterface.h"

using namespace Solid::Backends::UDisks2;

/* Static cache for DeviceBackends for all UDIs */
QMap<QString /* UDI */, DeviceBackend*> DeviceBackend::s_backends;

DeviceBackend* DeviceBackend::backendForUDI(const QString& udi)
{
    DeviceBackend *backend = 0;
    if (udi.isEmpty()) {
        return backend;
    }

    if (s_backends.contains(udi)) {
        backend = s_backends.value(udi);
    } else {
        backend = new DeviceBackend(udi);
        s_backends.insert(udi, backend);
    }

    return backend;
}

void DeviceBackend::destroyBackend(const QString& udi)
{
    if (s_backends.contains(udi)) {
        DeviceBackend *backend = s_backends.value(udi);
        s_backends.remove(udi);
        delete backend;
    }
}

DeviceBackend::DeviceBackend(const QString& udi)
    : m_udi(udi)
{
    //qDebug() << "Creating backend for device" << m_udi;
    m_device = new QDBusInterface(UD2_DBUS_SERVICE, m_udi,
                                  QString(), // no interface, we aggregate them
                                  QDBusConnection::systemBus(), this);

    if (m_device->isValid()) {
        QDBusConnection::systemBus().connect(UD2_DBUS_SERVICE, m_udi, DBUS_INTERFACE_PROPS, "PropertiesChanged", this,
                                            SLOT(slotPropertiesChanged(QString,QVariantMap,QStringList)));
        QDBusConnection::systemBus().connect(UD2_DBUS_SERVICE, UD2_DBUS_PATH, DBUS_INTERFACE_MANAGER, "InterfacesAdded",
                                            this, SLOT(slotInterfacesAdded(QDBusObjectPath,QVariantMapMap)));
        QDBusConnection::systemBus().connect(UD2_DBUS_SERVICE, UD2_DBUS_PATH, DBUS_INTERFACE_MANAGER, "InterfacesRemoved",
                                            this, SLOT(slotInterfacesRemoved(QDBusObjectPath,QStringList)));

        initInterfaces();
    }
}

DeviceBackend::~DeviceBackend()
{
    //qDebug() << "Destroying backend for device" << m_udi;
}

void DeviceBackend::initInterfaces()
{
    m_interfaces.clear();

    const QString xmlData = introspect();
    if (xmlData.isEmpty()) {
        qDebug() << m_udi << "has no interfaces!";
        return;
    }

    QDomDocument dom;
    dom.setContent(xmlData);

    QDomNodeList ifaceNodeList = dom.elementsByTagName("interface");
    for (int i = 0; i < ifaceNodeList.count(); i++) {
        QDomElement ifaceElem = ifaceNodeList.item(i).toElement();
        /* Accept only org.freedesktop.UDisks2.* interfaces so that when the device is unplugged,
         * m_interfaces goes empty and we can easily verify that the device is gone. */
        if (!ifaceElem.isNull() && ifaceElem.attribute("name").startsWith(UD2_DBUS_SERVICE)) {
            m_interfaces.append(ifaceElem.attribute("name"));
        }
    }

    //qDebug() << m_udi << "has interfaces:" << m_interfaces;
}

QStringList DeviceBackend::interfaces() const
{
    return m_interfaces;
}

const QString& DeviceBackend::udi() const
{
    return m_udi;
}

QVariant DeviceBackend::prop(const QString& key) const
{
    checkCache(key);
    return m_propertyCache.value(key);
}

bool DeviceBackend::propertyExists(const QString& key) const
{
    checkCache(key);
    /* checkCache() will put an invalid QVariant in cache when the property
     * does not exist, so check for validity, not for an actual presence. */
    return m_propertyCache.value(key).isValid();
}

QVariantMap DeviceBackend::allProperties() const
{
    QDBusMessage call = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, m_udi, DBUS_INTERFACE_PROPS, "GetAll");

    Q_FOREACH (const QString & iface, m_interfaces) {
        call.setArguments(QVariantList() << iface);
        QDBusPendingReply<QVariantMap> reply = QDBusConnection::systemBus().call(call);

        if (reply.isValid()) {
            m_propertyCache.unite(reply.value());
        } else {
            qWarning() << "Error getting props:" << reply.error().name() << reply.error().message();
        }
        //qDebug() << "After iface" << iface << ", cache now contains" << m_cache.size() << "items";
    }

    return m_propertyCache;
}

QString DeviceBackend::introspect() const
{
    QDBusMessage call = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, m_udi,
                                                    DBUS_INTERFACE_INTROSPECT, "Introspect");
    QDBusPendingReply<QString> reply = QDBusConnection::systemBus().call(call);

    if (reply.isValid())
        return reply.value();
    else {
        return QString();
    }
}

void DeviceBackend::checkCache(const QString& key) const
{
    if (m_propertyCache.isEmpty()) { // recreate the cache
        allProperties();
    }

    if (m_propertyCache.contains(key)) {
        return;
    }

    QVariant reply = m_device->property(key.toUtf8());
    m_propertyCache.insert(key, reply);

    if (!reply.isValid()) {
        /* Store the item in the cache anyway so next time we don't have to
         * do the DBus call to find out it does not exist but just check whether
         * prop(key).isValid() */
        //qDebug() << m_udi << ": property" << key << "does not exist";
    }
}

void DeviceBackend::slotPropertiesChanged(const QString& ifaceName, const QVariantMap& changedProps, const QStringList& invalidatedProps)
{
    //qDebug() << m_udi << "'s interface" << ifaceName << "changed props:";

    QMap<QString, int> changeMap;

    Q_FOREACH(const QString & key, invalidatedProps) {
        m_propertyCache.remove(key);
        changeMap.insert(key, Solid::GenericInterface::PropertyRemoved);
        //qDebug() << "\t invalidated:" << key;
    }

    QMapIterator<QString, QVariant> i(changedProps);
    while (i.hasNext()) {
        i.next();
        const QString key = i.key();
        m_propertyCache.insert(key, i.value());  // replace the value
        changeMap.insert(key, Solid::GenericInterface::PropertyModified);
        //qDebug() << "\t modified:" << key << ":" << m_propertyCache.value(key);
    }

    Q_EMIT propertyChanged(changeMap);
    Q_EMIT changed();
}

void DeviceBackend::slotInterfacesAdded(const QDBusObjectPath& object_path, const QVariantMapMap& interfaces_and_properties)
{
    if (object_path.path() != m_udi) {
        return;
    }

    Q_FOREACH(const QString & iface, interfaces_and_properties.keys()) {
        /* Don't store generic DBus interfaces */
        if (iface.startsWith(UD2_DBUS_SERVICE)) {
            m_interfaces.append(interfaces_and_properties.keys());
        }
    }
}

void DeviceBackend::slotInterfacesRemoved(const QDBusObjectPath& object_path, const QStringList& interfaces)
{
    if (object_path.path() != m_udi) {
        return;
    }

    Q_FOREACH(const QString & iface, interfaces) {
        m_interfaces.removeAll(iface);
    }
}
