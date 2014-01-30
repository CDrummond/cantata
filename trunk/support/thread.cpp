/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(ThreadCleaner, instance)
#endif
#include <QCoreApplication>
#include <QDebug>
#include <signal.h>
#include <unistd.h>

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

ThreadCleaner * ThreadCleaner::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static ThreadCleaner *instance=0;
    if(!instance) {
        instance=new ThreadCleaner;
    }
    return instance;
    #endif
}

void ThreadCleaner::stopAll()
{
    DBUG << "Remaining threads:" << threads.count();
    foreach (Thread *thread, threads) {
        DBUG << "Cleanup" << thread->objectName();
        disconnect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
    }

    foreach (Thread *thread, threads) {
        thread->stop();
    }

    QList<Thread *> stillRunning;
    foreach (Thread *thread, threads) {
        if (thread->wait(250)) {
            delete thread;
        } else {
            stillRunning.append(thread);
            DBUG << "Failed to close" << thread->objectName();
        }
    }

    // Terminate any still running threads...
    signal(SIGSEGV, segvHandler); // Ignore SEGV in case a thread throws an error...
    foreach (Thread *thread, stillRunning) {
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
