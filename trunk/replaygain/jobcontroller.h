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

#ifndef JOB_CONTROLLER
#define JOB_CONTROLLER

#include <QtCore/QThread>

class Job : public QThread
{
    Q_OBJECT
public:
    Job();
    virtual ~Job() {
    }

    void requestAbort() { abortRequested=true; }
    void stop();
    void setFinished(bool f);
    bool success() { return finished; }

Q_SIGNALS:
    void progress(int);
    void done();

protected:
    bool abortRequested;
    bool finished;
};

class JobController : public QObject
{
    Q_OBJECT
public:
    static JobController * self();
    JobController() {
    }

    void add(Job *job);
    void finishedWith(Job *job);
    void startJobs();
    void cancel();

private Q_SLOTS:
    void jobDone();

private:
    QList<Job *> active;
    QList<Job *> jobs;
};

#endif
