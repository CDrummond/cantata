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

#include "thread.h"
//#include <QDebug>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(ThreadCleaner, instance)
#endif

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

void ThreadCleaner::threadFinished()
{
    QThread *thread=qobject_cast<QThread *>(sender());
    if (thread) {
        thread->deleteLater();
    }
}

Thread::Thread(const QString &name, QObject *p)
    : QThread(p)
{
    setObjectName(name);
    connect(this, SIGNAL(finished()), ThreadCleaner::self(), SLOT(threadFinished()));
}

Thread::~Thread()
{
//    qWarning() << objectName() << "destroyed";
}
