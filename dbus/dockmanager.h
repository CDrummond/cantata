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

#ifndef DOCKMANAGER_H
#define DOCKMANAGER_H

#include <QtCore/QObject>
#include "dockmanagerinterface.h"
#include "dockiteminterface.h"

class QDBusServiceWatcher;

class DockManager : public QObject
{
    Q_OBJECT
public:
    DockManager(QObject *p);
    void setEnabled(bool e);
    bool enabled() const { return isEnabled; }

public Q_SLOTS:
    void setIcon(const QString &file);
    void serviceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private Q_SLOTS:
    void checkIfRunning();

private:
    void getDockItem();

private:
    bool isEnabled;
    net::launchpad::DockManager *mgr;
    net::launchpad::DockItem *item;
    QDBusServiceWatcher *watcher;
    QString currentIcon;
};

#endif
