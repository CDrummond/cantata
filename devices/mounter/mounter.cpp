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

#include "mounter.h"
#include "mounteradaptor.h"
#include "config.h"
#include <stdlib.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryFile>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
//#include <QtCore/QDebug>

Mounter::Mounter(QObject *p)
    : QObject(p)
    , timer(0)
{
    ok=true;
    QDBusConnection bus=QDBusConnection::systemBus();
    if (!bus.registerService("com.googlecode.cantata.mounter") || !bus.registerObject("/Mounter", this)) {
        ok=false;
    }
    new MounterAdaptor(this);
}

static inline bool mpOk(const QString &mp)
{
    return !mp.isEmpty() && mp.startsWith("/home/"); // ) && mp.contains("cantata");
}

static QString fixPath(const QString &dir)
{
    QString d(dir);

    if (!d.isEmpty() && !d.startsWith(QLatin1String("http://"))) {
        d.replace(QLatin1String("//"), QChar('/'));
    }
    d.replace(QLatin1String("/./"), QChar('/'));
    if (!d.isEmpty() && !d.endsWith('/')) {
        d+='/';
    }
    return d;
}

// Control via:
//     qdbus --system com.googlecode.cantata.mounter /Mounter com.googlecode.cantata.mounter.mount smb://workgroup\user:password@host:port/path?domain=domain mountPoint uid gid
void Mounter::mount(const QString &url, const QString &mountPoint, int uid, int gid)
{
    QUrl u(url);
    int st=-1;

    if (u.scheme()=="smb" && mpOk(mountPoint)) {
        QString user=u.userName();
        QString domain;
        QString password=u.password();
        int port=u.port();

        if (u.hasQueryItem("domain")) {
            domain=u.queryItemValue("domain");
        }

        QTemporaryFile *temp=0;

        if (!password.isEmpty()) {
            temp=new QTemporaryFile();
            if (temp && temp->open()) {
                QTextStream str(temp);
                if (!user.isEmpty()) {
                    str << "username=" << user << endl;
                }
                str << "password=" << password << endl;
                if (!domain.isEmpty()) {
                    str << "domain=" << domain << endl;
                }
            }
        }

        QString path=fixPath(u.host()+"/"+u.path());
        while (!path.startsWith("//")) {
            path="/"+path;
        }

//        qWarning() << "EXEC" << "mount.cifs" << path << mountPoint
//                                                         << "-o" <<
//                                                         (temp ? ("credentials="+temp->fileName()+",") : QString())+
//                                                         "uid="+QString::number(uid)+",gid="+QString::number(gid)+
//                                                         (445==port ? QString() : ",port="+QString::number(port))+
//                                                         (temp || user.isEmpty() ? QString() : (",username="+user))+
//                                                         (temp || domain.isEmpty() ? QString() : (",domain="+domain))+
//                                                         (temp ? QString() : ",password=");
        startTimer();
        st=QProcess::execute("mount.cifs", QStringList() << path << mountPoint
                             << "-o" <<
                             (temp ? ("credentials="+temp->fileName()+",") : QString())+
                             "uid="+QString::number(uid)+",gid="+QString::number(gid)+
                             (445==port ? QString() : ",port="+QString::number(port))+
                             (temp || user.isEmpty() ? QString() : (",username="+user))+
                             (temp || domain.isEmpty() ? QString() : (",domain="+domain))+
                             (temp ? QString() : ",password="));
        startTimer();

        if (temp) {
            delete temp;
        }
    }

    emit mountStatus(mountPoint, st);
}

// Control via:
//     qdbus --system com.googlecode.cantata.mounter /Mounter com.googlecode.cantata.mounter.umount mountPoint
void Mounter::umount(const QString &mountPoint)
{
    int st=-1;
    startTimer();
    if (mpOk(mountPoint)) {
        st=QProcess::execute("umount", QStringList() << mountPoint);
    }
    startTimer();
    emit umountStatus(mountPoint, st);
}

void Mounter::startTimer()
{
    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), SLOT(timeout()));
    }
    timer->start(10000);
}

void Mounter::timeout()
{
    qApp->exit();
}
