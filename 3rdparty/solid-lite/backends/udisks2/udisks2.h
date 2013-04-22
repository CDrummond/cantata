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

#ifndef SOLID_BACKENDS_UDISKS2_H
#define SOLID_BACKENDS_UDISKS2_H

#include <QMetaType>
#include <QtDBus>
#include <QVariant>
#include <QMap>
#include <QList>

typedef QList<QByteArray> QByteArrayList;
Q_DECLARE_METATYPE(QByteArrayList)

typedef QMap<QString,QVariantMap> QVariantMapMap;
Q_DECLARE_METATYPE(QVariantMapMap)

typedef QMap<QDBusObjectPath, QVariantMapMap> DBUSManagerStruct;
Q_DECLARE_METATYPE(DBUSManagerStruct)

/* UDisks2 */
#define UD2_DBUS_SERVICE                 "org.freedesktop.UDisks2"
#define UD2_DBUS_PATH                    "/org/freedesktop/UDisks2"
#define UD2_UDI_DISKS_PREFIX             "/org/freedesktop/UDisks2"
#define UD2_DBUS_PATH_MANAGER            "/org/freedesktop/UDisks2/Manager"
#define UD2_DBUS_PATH_DRIVES             "/org/freedesktop/UDisks2/drives/"
#define UD2_DBUS_PATH_JOBS               "/org/freedesktop/UDisks2/jobs/"
#define DBUS_INTERFACE_PROPS             "org.freedesktop.DBus.Properties"
#define DBUS_INTERFACE_INTROSPECT        "org.freedesktop.DBus.Introspectable"
#define DBUS_INTERFACE_MANAGER           "org.freedesktop.DBus.ObjectManager"
#define UD2_DBUS_INTERFACE_BLOCK         "org.freedesktop.UDisks2.Block"
#define UD2_DBUS_INTERFACE_DRIVE         "org.freedesktop.UDisks2.Drive"
#define UD2_DBUS_INTERFACE_PARTITION     "org.freedesktop.UDisks2.Partition"
#define UD2_DBUS_INTERFACE_PARTITIONTABLE   "org.freedesktop.UDisks2.PartitionTable"
#define UD2_DBUS_INTERFACE_FILESYSTEM    "org.freedesktop.UDisks2.Filesystem"
#define UD2_DBUS_INTERFACE_ENCRYPTED     "org.freedesktop.UDisks2.Encrypted"
#define UD2_DBUS_INTERFACE_SWAP          "org.freedesktop.UDisks2.Swapspace"
#define UD2_DBUS_INTERFACE_LOOP          "org.freedesktop.UDisks2.Loop"

/* errors */
#define UD2_ERROR_UNAUTHORIZED            "org.freedesktop.PolicyKit.Error.NotAuthorized"
#define UD2_ERROR_BUSY                    "org.freedesktop.UDisks2.Error.DeviceBusy"
#define UD2_ERROR_FAILED                  "org.freedesktop.UDisks2.Error.Failed"
#define UD2_ERROR_CANCELED                "org.freedesktop.UDisks2.Error.Cancelled"
#define UD2_ERROR_INVALID_OPTION          "org.freedesktop.UDisks2.Error.OptionNotPermitted"
#define UD2_ERROR_MISSING_DRIVER          "org.freedesktop.UDisks2.Error.NotSupported"

#define UD2_ERROR_ALREADY_MOUNTED         "org.freedesktop.UDisks2.Error.AlreadyMounted"
#define UD2_ERROR_NOT_MOUNTED             "org.freedesktop.UDisks2.Error.NotMounted"
#define UD2_ERROR_MOUNTED_BY_OTHER_USER   "org.freedesktop.UDisks2.Error.MountedByOtherUser"
#define UD2_ERROR_ALREADY_UNMOUNTING      "org.freedesktop.UDisks2.Error.AlreadyUnmounting"
#define UD2_ERROR_TIMED_OUT               "org.freedesktop.UDisks2.Error.Timedout"
#define UD2_ERROR_WOULD_WAKEUP            "org.freedesktop.UDisks2.Error.WouldWakeup"
#define UD2_ERROR_ALREADY_CANCELLED       "org.freedesktop.UDisks2.Error.AlreadyCancelled"

#define UD2_ERROR_NOT_AUTHORIZED          "org.freedesktop.UDisks2.Error.NotAuthorized"
#define UD2_ERROR_NOT_AUTHORIZED_CAN_OBTAIN  "org.freedesktop.UDisks2.Error.NotAuthorizedCanObtain"
#define UD2_ERROR_NOT_AUTHORIZED_DISMISSED   "org.freedesktop.UDisks2.Error.NotAuthorizedDismissed"

#endif // SOLID_BACKENDS_UDISKS2_H
