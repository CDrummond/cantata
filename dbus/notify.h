/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef NOTIFY_H
#define NOTIFY_H

#include <QObject>
#include <QDateTime>

class QImage;
class QDBusPendingCallWatcher;
class QDBusArgument;
class OrgFreedesktopNotificationsInterface;

QDBusArgument& operator<< (QDBusArgument &arg, const QImage &image);
const QDBusArgument& operator>> (const QDBusArgument &arg, QImage &image);

class Notify : public QObject
{
    Q_OBJECT

public:
    Notify(QObject *p);

    virtual ~Notify() {
    }

    void show(const QString &title, const QString &text, const QImage &img);
    
private Q_SLOTS:
    void callFinished(QDBusPendingCallWatcher *watcher);

private:
    QDateTime lastTime;
    int lastId;
    OrgFreedesktopNotificationsInterface *iface;
};

#endif
