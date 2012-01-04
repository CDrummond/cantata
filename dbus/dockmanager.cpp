/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "dockmanager.h"

DockManager::DockManager(QObject *p)
    : QObject(p)
    , isEnabled(false)
    , mgr(0)
    , item(0)
    , watcher(0)
{
}

void DockManager::setEnabled(bool e)
{
    if (e==isEnabled) {
        return;
    }

    isEnabled=e;
    if(!isEnabled) {
        if (watcher) {
            disconnect(watcher, SIGNAL(serviceOwnerChanged(QString, QString, QString)), this, SLOT(serviceOwnerChanged(QString, QString, QString)));
            watcher->deleteLater();
            watcher=0;
        }
        if (item) {
            QString old=currentIcon;
            setIcon(QString());
            currentIcon=old;
            item->deleteLater();
            item = 0;
        }
        if (mgr) {
            mgr->deleteLater();
            mgr = 0;
        }
    } else {
        watcher = new QDBusServiceWatcher(this);
        watcher->setConnection(QDBusConnection::sessionBus());
        watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
        watcher->addWatchedService(net::launchpad::DockManager::staticInterfaceName());
        connect(watcher, SIGNAL(serviceOwnerChanged(QString, QString, QString)), this, SLOT(serviceOwnerChanged(QString, QString, QString)));

        QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(net::launchpad::DockManager::staticInterfaceName());
        if (reply.isValid() && reply.value()) {
            serviceOwnerChanged(net::launchpad::DockManager::staticInterfaceName(), QString(), QLatin1String("X"));
        }
    }
}

void DockManager::setIcon(const QString &file)
{
    currentIcon=file;
    // If we are unregistering an icon, and the dock manager item does not exist - dont bother connecting!
    if (file.isEmpty() && !item) {
        return;
    }
    getDockItem();

    if (!item) {
        return;
    }

    QVariantMap settings;
    settings.insert("icon-file", file);
    item->UpdateDockItem(settings);
}

void DockManager::serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(name)
    if (newOwner.isEmpty()) {
        if (item) {
            item->deleteLater();
            item = 0;
        }
        if (mgr) {
            mgr->deleteLater();
            mgr = 0;
        }
    } else if (oldOwner.isEmpty()) {
        mgr = new net::launchpad::DockManager(net::launchpad::DockManager::staticInterfaceName(), "/net/launchpad/DockManager", QDBusConnection::sessionBus(), this);
        if (!currentIcon.isEmpty()) {
            setIcon(currentIcon);
        }
    }
}

void DockManager::getDockItem()
{
    if (!mgr || item) {
        return;
    }

    QDBusReply<QList<QDBusObjectPath> > reply = mgr->GetItems();

    if (reply.isValid()) {
        QList<QDBusObjectPath> paths = reply.value();
        foreach(const QDBusObjectPath &path, paths) {
            item = new net::launchpad::DockItem(net::launchpad::DockManager::staticInterfaceName(), path.path(), QDBusConnection::sessionBus(), this);
            if (item->desktopFile().endsWith("cantata.desktop")) {
                break;
            } else {
                item->deleteLater();
                item=0;
            }
        }
    }
}
