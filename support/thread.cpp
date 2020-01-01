/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "thread.h"
#include "globalstatic.h"
#include "utils.h"
#include <QCoreApplication>
#include <QtGlobal>
#include <QTimer>
#include <QDebug>
#include <signal.h>
#ifndef _MSC_VER 
#include <unistd.h>
#endif

static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void ThreadCleaner::enableDebug()
{
    debugEnabled=true;
}

static void segvHandler(int i)
{
    if (debugEnabled) qWarning() << "SEGV handler called";
    _exit(i);
}

GLOBAL_STATIC(ThreadCleaner, instance)

void ThreadCleaner::stopAll()
{
    DBUG << "Remaining threads:" << threads.count();
    for (Thread *thread: threads) {
        DBUG << "Cleanup" << thread->objectName();
        disconnect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
    }

    for (Thread *thread: threads) {
        thread->stop();
    }

    QList<Thread *> stillRunning;
    for (Thread *thread: threads) {
        if (thread->wait(250)) {
            delete thread;
        } else {
            stillRunning.append(thread);
            DBUG << "Failed to close" << thread->objectName();
        }
    }

    // Terminate any still running threads...
    signal(SIGSEGV, segvHandler); // Ignore SEGV in case a thread throws an error...
    for (Thread *thread: stillRunning) {
        thread->terminate();
    }
}

void ThreadCleaner::threadFinished()
{
    Thread *thread=qobject_cast<Thread *>(sender());
    if (thread) {
        thread->deleteLater();
        threads.removeAll(thread);
        DBUG << "Thread finished" << thread->objectName() << "Total threads:" << threads.count();
    }
}

void ThreadCleaner::add(Thread *thread)
{
    threads.append(thread);
    connect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
    DBUG << "Thread created" << thread->objectName() << "Total threads:" << threads.count();
}

Thread::Thread(const QString &name, QObject *p)
    : QThread(p)
{
    setObjectName(name);
    ThreadCleaner::self()->add(this);
}

Thread::~Thread()
{
    DBUG << objectName() << "destroyed";
}

void Thread::run()
{
    Utils::initRand();
    QThread::run();
}

QTimer * Thread::createTimer(QObject *parent)
{
    QTimer *timer=new QTimer(parent ? parent : this);
    connect(this, SIGNAL(finished()), timer, SLOT(stop()));
    return timer;
}

void Thread::deleteTimer(QTimer *timer)
{
    if (timer) {
        disconnect(this, SIGNAL(finished()), timer, SLOT(stop()));
        timer->deleteLater();
    }
}

#include "moc_thread.cpp"
