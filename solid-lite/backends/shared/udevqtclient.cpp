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

#include "udevqt.h"
// #include "udevqt_p.h"

#include <sys/stat.h>
#include <QtCore/QSocketNotifier>

namespace UdevQt {

ClientPrivate::ClientPrivate(Client *q_)
    : udev(0), monitor(0), q(q_), monitorNotifier(0)
{
}

ClientPrivate::~ClientPrivate()
{
    udev_unref(udev);
    delete monitorNotifier;

    if (monitor)
        udev_monitor_unref(monitor);
}

void ClientPrivate::init(const QStringList &subsystemList, ListenToWhat what)
{
    udev = udev_new();

    if (what != ListenToNone) {
        setWatchedSubsystems(subsystemList);
    }
}

void ClientPrivate::setWatchedSubsystems(const QStringList &subsystemList)
{
    // create a listener
    struct udev_monitor *newM = udev_monitor_new_from_netlink(udev, "udev");

    if (!newM) {
        qWarning("UdevQt: unable to create udev monitor connection");
        return;
    }

    // apply our filters; an empty list means listen to everything
    foreach (const QString& subsysDevtype, subsystemList) {
        int ix = subsysDevtype.indexOf("/");

        if (ix > 0) {
            QByteArray subsystem = subsysDevtype.left(ix).toLatin1();
            QByteArray devType = subsysDevtype.mid(ix + 1).toLatin1();
            udev_monitor_filter_add_match_subsystem_devtype(newM, subsystem.constData(), devType.constData());
        } else {
            udev_monitor_filter_add_match_subsystem_devtype(newM, subsysDevtype.toLatin1().constData(), NULL);
        }
    }

    // start the new monitor receiving
    udev_monitor_enable_receiving(newM);
    QSocketNotifier *sn = new QSocketNotifier(udev_monitor_get_fd(newM), QSocketNotifier::Read);
    QObject::connect(sn, SIGNAL(activated(int)), q, SLOT(_uq_monitorReadyRead(int)));

    // kill any previous monitor
    delete monitorNotifier;
    if (monitor)
        udev_monitor_unref(monitor);

    // and save our new one
    monitor = newM;
    monitorNotifier = sn;
    watchedSubsystems = subsystemList;
}

void ClientPrivate::_uq_monitorReadyRead(int fd)
{
    Q_UNUSED(fd);
    monitorNotifier->setEnabled(false);
    struct udev_device *dev = udev_monitor_receive_device(monitor);
    monitorNotifier->setEnabled(true);

    if (!dev)
        return;

    Device device(new DevicePrivate(dev, false));

    QByteArray action(udev_device_get_action(dev));
    if (action == "add") {
        emit q->deviceAdded(device);
    } else if (action == "remove") {
        emit q->deviceRemoved(device);
    } else if (action == "change") {
        emit q->deviceChanged(device);
    } else if (action == "online") {
        emit q->deviceOnlined(device);
    } else  if (action == "offline") {
        emit q->deviceOfflined(device);
    } else {
        qWarning("UdevQt: unhandled device action \"%s\"", action.constData());
    }
}

DeviceList ClientPrivate::deviceListFromEnumerate(struct udev_enumerate *en)
{
    DeviceList ret;
    struct udev_list_entry *list, *entry;

    udev_enumerate_scan_devices(en);
    list = udev_enumerate_get_list_entry(en);
    udev_list_entry_foreach(entry, list) {
        struct udev_device *ud = udev_device_new_from_syspath(udev_enumerate_get_udev(en),
                                        udev_list_entry_get_name(entry));

        if (!ud)
            continue;

        ret << Device(new DevicePrivate(ud, false));
    }

    udev_enumerate_unref(en);

    return ret;
}


Client::Client(QObject *parent)
    : QObject(parent)
    , d(new ClientPrivate(this))
{
    d->init(QStringList(), ClientPrivate::ListenToNone);
}

Client::Client(const QStringList& subsystemList, QObject *parent)
    : QObject(parent)
    , d(new ClientPrivate(this))
{
    d->init(subsystemList, ClientPrivate::ListenToList);
}

Client::~Client()
{
    delete d;
}

QStringList Client::watchedSubsystems() const
{
    // we're watching a specific list
    if (!d->watchedSubsystems.isEmpty())
        return d->watchedSubsystems;

    // we're not watching anything
    if (!d->monitor)
        return QStringList();

    // we're watching everything: figure out what "everything" currently is
    // we don't cache it, since it may be subject to change, depending on hotplug
    struct udev_enumerate *en = udev_enumerate_new(d->udev);
    udev_enumerate_scan_subsystems(en);
    QStringList s = listFromListEntry(udev_enumerate_get_list_entry(en));
    udev_enumerate_unref(en);
    return s;
}

void Client::setWatchedSubsystems(const QStringList &subsystemList)
{
    d->setWatchedSubsystems(subsystemList);
}

DeviceList Client::devicesByProperty(const QString &property, const QVariant &value)
{
    struct udev_enumerate *en = udev_enumerate_new(d->udev);

    if (value.isValid()) {
        udev_enumerate_add_match_property(en, property.toLatin1().constData(), value.toString().toLatin1().constData());
    } else {
        udev_enumerate_add_match_property(en, property.toLatin1().constData(), NULL);
    }

    return d->deviceListFromEnumerate(en);
}

DeviceList Client::allDevices()
{
    struct udev_enumerate *en = udev_enumerate_new(d->udev);
    return d->deviceListFromEnumerate(en);
}

DeviceList Client::devicesBySubsystem(const QString &subsystem)
{
    struct udev_enumerate *en = udev_enumerate_new(d->udev);

    udev_enumerate_add_match_subsystem(en, subsystem.toLatin1().constData());
    return d->deviceListFromEnumerate(en);
}

Device Client::deviceByDeviceFile(const QString &deviceFile)
{
    struct stat sb;

    if (stat(deviceFile.toLatin1().constData(), &sb) != 0)
        return Device();

    struct udev_device *ud = 0;

    if (S_ISBLK(sb.st_mode))
        ud = udev_device_new_from_devnum(d->udev, 'b', sb.st_rdev);
    else if (S_ISCHR(sb.st_mode))
        ud = udev_device_new_from_devnum(d->udev, 'c', sb.st_rdev);

    if (!ud)
        return Device();

    return Device(new DevicePrivate(ud, false));
}

Device Client::deviceBySysfsPath(const QString &sysfsPath)
{
    struct udev_device *ud = udev_device_new_from_syspath(d->udev, sysfsPath.toLatin1().constData());

    if (!ud)
        return Device();

    return Device(new DevicePrivate(ud, false));
}

Device Client::deviceBySubsystemAndName(const QString &subsystem, const QString &name)
{
    struct udev_device *ud = udev_device_new_from_subsystem_sysname(d->udev,
                                    subsystem.toLatin1().constData(),
                                    name.toLatin1().constData());

    if (!ud)
        return Device();

    return Device(new DevicePrivate(ud, false));
}

}

//#include "udevqt.moc"
