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

#ifndef SOLID_BACKENDS_UDISKS_H
#define SOLID_BACKENDS_UDISKS_H

/* UDisks */
#define UD_DBUS_SERVICE                 "org.freedesktop.UDisks"
#define UD_DBUS_PATH                    "/org/freedesktop/UDisks"
#define UD_DBUS_INTERFACE_DISKS         "org.freedesktop.UDisks"
#define UD_DBUS_INTERFACE_DISKS_DEVICE  "org.freedesktop.UDisks.Device"
#define UD_UDI_DISKS_PREFIX             "/org/freedesktop/UDisks"

/* errors */
#define UD_ERROR_UNAUTHORIZED            "org.freedesktop.PolicyKit.Error.NotAuthorized"
#define UD_ERROR_BUSY                    "org.freedesktop.UDisks.Error.Busy"
#define UD_ERROR_FAILED                  "org.freedesktop.UDisks.Error.Failed"
#define UD_ERROR_CANCELED                "org.freedesktop.UDisks.Error.Cancelled"
#define UD_ERROR_INVALID_OPTION          "org.freedesktop.UDisks.Error.InvalidOption"
#define UD_ERROR_MISSING_DRIVER          "org.freedesktop.UDisks.Error.FilesystemDriverMissing"

#endif // SOLID_BACKENDS_UDISKS_H
