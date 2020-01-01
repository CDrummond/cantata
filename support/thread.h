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

#ifndef THREAD_H
#define THREAD_H

#include <QThread>

class QTimer;
// ThreadCleaner *needs* to reside in the GUI thread. When a 'Thread' is created it will connect
// its finished signal to threadFinished(), this then calls deleteLater() to ensure that the
// thread is finished before it is deleted - and is deleted in the gui thread.
class Thread;
class ThreadCleaner : public QObject
{
    Q_OBJECT
public:
    static void enableDebug();
    static ThreadCleaner * self();
    ThreadCleaner() { }
    ~ThreadCleaner() override { }

    // This function must *ONLY* be called from GUI thread...
    void stopAll();

public Q_SLOTS:
    void threadFinished();

private:
    void add(Thread *thread);

private:
    QList<Thread *> threads;
    friend class Thread;
};

class Thread : public QThread
{
    Q_OBJECT
public:
    Thread(const QString &name, QObject *p=nullptr);
    ~Thread() override;

    // Make QThread::msleep accessible!
    using QThread::msleep;

    void run() override;

    QTimer * createTimer(QObject *parent=nullptr);
    void deleteTimer(QTimer *timer);

public Q_SLOTS:
    void stop() { quit(); }
};

#endif
