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
#include <QtCore/QDebug>
#include <sys/types.h>
#include <signal.h>

Mounter::Mounter(QObject *p)
    : QObject(p)
    , timer(0)
    , procCount(0)
{
    new MounterAdaptor(this);
    QDBusConnection bus=QDBusConnection::systemBus();
    if (!bus.registerService("com.googlecode.cantata.mounter") || !bus.registerObject("/Mounter", this)) {
        QTimer::singleShot(0, qApp, SLOT(quit()));
    }
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
void Mounter::mount(const QString &url, const QString &mountPoint, int uid, int gid, int pid)
{
    if (calledFromDBus()) {
        registerPid(pid);
    }

    qWarning() << url << mountPoint << uid << gid;
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
//                                                         (445==port || port<1 ? QString() : ",port="+QString::number(port))+
//                                                         (temp || user.isEmpty() ? QString() : (",username="+user))+
//                                                         (temp || domain.isEmpty() ? QString() : (",domain="+domain))+
//                                                         (temp ? QString() : ",password=");
        QProcess *proc=new QProcess(this);
        connect(proc, SIGNAL(finished(int)), SLOT(mountResult(int)));
        proc->setProperty("mp", mountPoint);
        proc->setProperty("pid", pid);
        proc->start(INSTALL_PREFIX"/share/cantata/mount.cifs.wrapper", QStringList() << path << mountPoint
                    << "-o" <<
                    (temp ? ("credentials="+temp->fileName()+",") : QString())+
                    "uid="+QString::number(uid)+",gid="+QString::number(gid)+
                    (445==port || port<1 ? QString() : ",port="+QString::number(port))+
                    (temp || user.isEmpty() ? QString() : (",username="+user))+
                    (temp || domain.isEmpty() ? QString() : (",domain="+domain))+
                    (temp ? QString() : ",password="), QIODevice::WriteOnly);
        if (temp) {
            tempFiles.insert(proc, temp);
        }
        procCount++;
        return;
    }
    emit mountStatus(mountPoint, pid, st);
}

// Control via:
//     qdbus --system com.googlecode.cantata.mounter /Mounter com.googlecode.cantata.mounter.umount mountPoint
void Mounter::umount(const QString &mountPoint, int pid)
{
    if (calledFromDBus()) {
        registerPid(pid);
    }

    if (mpOk(mountPoint)) {
        QProcess *proc=new QProcess(this);
        connect(proc, SIGNAL(finished(int)), SLOT(umountResult(int)));
        proc->start("umount", QStringList() << mountPoint);
        proc->setProperty("mp", mountPoint);
        proc->setProperty("pid", pid);
        procCount++;
    } else {
        emit umountStatus(mountPoint, pid, -1);
    }
}

void Mounter::mountResult(int st)
{
    QProcess *proc=dynamic_cast<QProcess *>(sender());
    qWarning() << "MOUNT RESULT" << st << (void *)proc;
    if (proc) {
        procCount--;
        proc->close();
        proc->deleteLater();
        if (tempFiles.contains(proc)) {
            tempFiles[proc]->close();
            tempFiles[proc]->deleteLater();
            tempFiles.remove(proc);
        }
        emit mountStatus(proc->property("mp").toString(), proc->property("pid").toInt(), st);
    }
    startTimer();
}


void Mounter::umountResult(int st)
{
    QProcess *proc=dynamic_cast<QProcess *>(sender());
    if (proc) {
        procCount--;
        proc->close();
        proc->deleteLater();
        emit umountStatus(proc->property("mp").toString(), proc->property("pid").toInt(), st);
    }
    startTimer();
}

void Mounter::startTimer()
{
    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), SLOT(timeout()));
    }
    timer->start(30000);
}

void Mounter::registerPid(int pid)
{
    pids.insert(pid);
    startTimer();
}

void Mounter::timeout()
{
    if (procCount!=0) {
        startTimer();
        return;
    }

    QSet<int> running;

    foreach (int p, pids) {
        if (0==kill(p, 0)) {
            running.insert(p);
        }
    }

    pids=running;

    if (pids.isEmpty()) {
        qApp->exit();
        QMap<QObject *, QTemporaryFile *>::ConstIterator it(tempFiles.constBegin());
        QMap<QObject *, QTemporaryFile *>::ConstIterator end(tempFiles.constEnd());
        for (; it!=end; ++it) {
            it.value()->close();
            delete it.value();
        }
        tempFiles.clear();
    } else {
        startTimer();
    }
}
