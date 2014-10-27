/*
    Copyright 2009 Benjamin K. Stuhl <bks24@cornell.edu>

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

#ifndef UDEVQT_P_H
#define UDEVQT_P_H

extern "C"
{
#define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#include <libudev.h>
}

class QByteArray;
class QSocketNotifier;

namespace UdevQt
{

class DevicePrivate
{
    public:
        DevicePrivate(struct udev_device *udev_, bool ref = true);
        ~DevicePrivate();
        DevicePrivate &operator=(const DevicePrivate& other);

        QString decodePropertyValue(const QByteArray &encoded) const;

        struct udev_device *udev;
};


class ClientPrivate
{
    public:
        enum ListenToWhat { ListenToList, ListenToNone };

        ClientPrivate(Client *q_);
        ~ClientPrivate();

        void init(const QStringList &subsystemList, ListenToWhat what);
        void setWatchedSubsystems(const QStringList &subsystemList);
        void _uq_monitorReadyRead(int fd);
        DeviceList deviceListFromEnumerate(struct udev_enumerate *en);

        struct udev *udev;
        struct udev_monitor *monitor;
        Client *q;
        QSocketNotifier *monitorNotifier;
        QStringList watchedSubsystems;
};

inline QStringList listFromListEntry(struct udev_list_entry *list)
{
    QStringList ret;
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, list) {
        ret << QString::fromLatin1(udev_list_entry_get_name(entry));
    }
    return ret;
}

}

#endif
