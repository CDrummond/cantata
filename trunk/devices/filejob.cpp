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

#include "filejob.h"
#include "utils.h"
#include <QtCore/QFile>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(FileScheduler, instance)
#endif

FileScheduler * FileScheduler::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static FileScheduler *instance=0;
    if(!instance) {
        instance=new FileScheduler;
    }
    return instance;
    #endif
}

FileScheduler::FileScheduler()
    : thread(0)
{
}

void FileScheduler::addJob(FileJob *job)
{
    if (!thread) {
        thread=new QThread();
        moveToThread(thread);
        thread->start();
    }
    job->moveToThread(thread);
}

void FileScheduler::stop()
{
    if (thread) {
        Utils::stopThread(thread);
        thread=0;
    }
}

void FileJob::start()
{
    QTimer::singleShot(0, this, SLOT(run()));
}

void FileJob::setPercent(int pc)
{
    if (pc!=progressPercent) {
        progressPercent=pc;
        emit percent(progressPercent);
    }
}

static const int constChunkSize=32*1024;

void CopyJob::run()
{
    QFile src(srcFile);

    if (!src.open(QIODevice::ReadOnly)) {
        emit result(StatusFailed);
        return;
    }

    QFile dest(destFile);
    if (!dest.open(QIODevice::WriteOnly)) {
        emit result(StatusFailed);
        return;
    }

    char buffer[constChunkSize];
    qint64 totalBytes = src.size();
    qint64 readPos = 0;
    qint64 bytesRead = 0;

    do {
        if (stopRequested) {
            emit result(StatusCancelled);
            return;
        }
        bytesRead = src.read(buffer, constChunkSize);
        readPos+=bytesRead;
        if (bytesRead<0) {
            emit result(StatusFailed);
            return;
        }

        if (stopRequested) {
            emit result(StatusCancelled);
            return;
        }

        qint64 writePos=0;
        do {
            qint64 bytesWritten = dest.write(&buffer[writePos], bytesRead - writePos);
            if (stopRequested) {
                emit result(StatusCancelled);
                return;
            }
            if (-1==bytesWritten) {
                emit result(StatusFailed);
                return;
            }
            writePos+=bytesWritten;
        } while (writePos<bytesRead);

        setPercent(((readPos+bytesRead)*100.0)/totalBytes);
        if (src.atEnd()) {
            break;
        }
    } while ((readPos+bytesRead)<totalBytes);

    emit result(StatusOk);
}

void DeleteJob::run()
{
    emit result(QFile::remove(fileName) ? StatusOk : StatusFailed);
    emit percent(100);
}

