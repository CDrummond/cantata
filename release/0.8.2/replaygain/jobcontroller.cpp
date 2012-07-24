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

#include "jobcontroller.h"
#include "utils.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(JobController, instance)
#endif

static const int constMaxActive=8;

void Job::stop()
{
    abortRequested=true;
    Utils::stopThread(this);
    deleteLater();
}

void Job::setFinished(bool f)
{
    finished=f;
    emit done();
}

JobController * JobController::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static JobController *instance=0;
    if(!instance) {
        instance=new JobController;
    }
    return instance;
    #endif
}

void JobController::add(Job *job)
{
    jobs.append(job);
    startJobs();
}

void JobController::finishedWith(Job *job)
{
    active.removeAll(job);
    jobs.removeAll(job);
    job->stop();
}

void JobController::startJobs()
{
    while (active.count()<constMaxActive && !jobs.isEmpty()) {
        Job *job=jobs.takeAt(0);
        active.append(job);
        connect(job, SIGNAL(done()), this, SLOT(jobDone()), Qt::QueuedConnection);
        job->start();
    }
}

void JobController::cancel() {
    foreach (Job *j, active) {
        disconnect(j, SIGNAL(done()), this, SLOT(jobDone()));
        j->stop();
    }
    active.clear();
    foreach (Job *j, jobs) {
        j->deleteLater();
    }
    jobs.clear();
}

void JobController::jobDone()
{
    QObject *s=sender();

    if (s && dynamic_cast<Job *>(s)) {
        active.removeAll(static_cast<Job *>(s));
    }
    startJobs();
}

