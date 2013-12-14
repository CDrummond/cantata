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

#ifndef UDISKSDEVICEBACKEND_H
#define UDISKSDEVICEBACKEND_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusInterface>
#include <QStringList>

#include "udisks2.h"

namespace Solid {
namespace Backends {
namespace UDisks2 {

class DeviceBackend: public QObject {

    Q_OBJECT

  public:
    static DeviceBackend* backendForUDI(const QString &udi, bool create = true);
    static void destroyBackend(const QString &udi);

    DeviceBackend(const QString &udi);
    ~DeviceBackend();

    QVariant prop(const QString &key) const;
    bool propertyExists(const QString &key) const;
    QVariantMap allProperties() const;

    QStringList interfaces() const;
    const QString & udi() const;

    void invalidateProperties();
  Q_SIGNALS:
    void propertyChanged(const QMap<QString, int> &changeMap);
    void changed();

  private Q_SLOTS:
    void slotInterfacesAdded(const QDBusObjectPath &object_path, const QVariantMapMap &interfaces_and_properties);
    void slotInterfacesRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void slotPropertiesChanged(const QString &ifaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);

  private:
    void initInterfaces();
    QString introspect() const;
    void checkCache(const QString &key) const;

    QDBusInterface *m_device;

    mutable QVariantMap m_propertyCache;
    QStringList m_interfaces;
    QString m_udi;

    static QMap<QString, DeviceBackend*> s_backends;

};

} /* namespace UDisks2 */
} /* namespace Backends */
} /* namespace Solid */

#endif /* UDISKSDEVICEBACKEND_H */
