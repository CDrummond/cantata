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

#ifndef MOUNTER_H
#define MOUNTER_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtDBus>

class QTimer;
class QTemporaryFile;

class Mounter : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.googlecode.cantata.mounter")

public:
    Mounter(QObject *p=0);
    ~Mounter() { }

public Q_SLOTS:
     Q_NOREPLY void mount(const QString &url, const QString &mountPoint, int uid, int gid, int pid);
     Q_NOREPLY void umount(const QString &mountPoint, int pid);

private:
    void startTimer();
    void registerPid(int pid);

private Q_SLOTS:
    void mountResult(int st);
    void umountResult(int st);
    void timeout();

Q_SIGNALS:
    void mountStatus(const QString &src, int pid, int st);
    void umountStatus(const QString &src, int pid, int st);

private:
    QTimer *timer;
    int procCount;
    QMap<QObject *, QTemporaryFile *> tempFiles;
    QSet<int> pids;
};

#endif

