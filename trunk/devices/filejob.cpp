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

#include "filejob.h"
#include "utils.h"
#include "device.h"
#include "songview.h"
#include "covers.h"
#include "thread.h"
#include <QFile>
#include <QTimer>
#include <QTemporaryFile>
#include <QDebug>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(FileThread, instance)
#endif

FileThread * FileThread::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static FileThread *instance=0;
    if(!instance) {
        instance=new FileThread;
    }
    return instance;
    #endif
}

FileThread::FileThread()
    : thread(0)
{
}

FileThread::~FileThread()
{
    stop();
}

void FileThread::addJob(FileJob *job)
{
    if (!thread) {
        thread=new Thread(metaObject()->className());
        thread->start();
    }
    job->moveToThread(thread);
}

void FileThread::stop()
{
    if (thread) {
        thread->stop();
        thread=0;
    }
}

FileJob::FileJob()
    : stopRequested(false)
    , progressPercent(0)
{
    FileThread::self()->addJob(this);
    // Cant call deleteLater here, as in the device's xxResult() slots "sender()" returns
    // null. Therefore, xxResult() slots need to call finished()
    //connect(this, SIGNAL(result(int)), SLOT(deleteLater()));
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

CopyJob::~CopyJob()
{
    if (temp) {
        temp->remove();
        delete temp;
    }
}

static const int constChunkSize=32*1024;

QString CopyJob::updateTagsLocal()
{
    // First, if we are going to update tags (e.g. to fix various artists), then check if we want to do that locally, before
    // copying to device. For UMS devices, we just modify on device, but for remote (e.g. sshfs) then it'd be better to do locally :-)
    if (copyOpts&OptsFixLocal && (copyOpts&OptsApplyVaFix || copyOpts&OptsUnApplyVaFix || Device::constEmbedCover==deviceOpts.coverName)) {
        song.file=srcFile;
        temp=Device::copySongToTemp(song);
        if (!temp) {
            emit result(Device::FailedToUpdateTags);
            return QString();
        }
        if ((copyOpts&OptsApplyVaFix || copyOpts&OptsUnApplyVaFix) && !Device::fixVariousArtists(temp->fileName(), song, copyOpts&OptsApplyVaFix)) {
            emit result(Device::FailedToUpdateTags);
            return QString();
        }
        if (Device::constEmbedCover==deviceOpts.coverName) {
            Device::embedCover(temp->fileName(), song, deviceOpts.coverMaxSize);
        }
        return temp->fileName();
    }
    return srcFile;
}

void CopyJob::updateTagsDest()
{
    if (!stopRequested && !(copyOpts&OptsFixLocal) && (copyOpts&OptsApplyVaFix || copyOpts&OptsUnApplyVaFix || Device::constEmbedCover==deviceOpts.coverName)) {
        if (copyOpts&OptsApplyVaFix || copyOpts&OptsUnApplyVaFix) {
            Device::fixVariousArtists(destFile, song, copyOpts&OptsApplyVaFix);
        }
        if (!stopRequested && Device::constEmbedCover==deviceOpts.coverName) {
            Device::embedCover(destFile, song, deviceOpts.coverMaxSize);
        }
    }
}

void CopyJob::copyCover(const QString &origSrcFile)
{
    if (!stopRequested && Device::constNoCover!=deviceOpts.coverName && Device::constEmbedCover!=deviceOpts.coverName) {
        song.file=destFile;
        copiedCover=Covers::copyCover(song, Utils::getDir(origSrcFile), Utils::getDir(destFile), deviceOpts.coverName, deviceOpts.coverMaxSize);
    }
}

void CopyJob::run()
{
    QString origSrcFile(srcFile);
    srcFile=updateTagsLocal();
    if (srcFile.isEmpty()) {
        return;
    }

    if (stopRequested) {
        emit result(Device::Cancelled);
        return;
    }

    QFile src(srcFile);

    if (!src.open(QIODevice::ReadOnly)) {
        emit result(Device::ReadFailed);
        return;
    }

    QFile dest(destFile);
    if (!dest.open(QIODevice::WriteOnly)) {
        emit result(Device::WriteFailed);
        return;
    }

    char buffer[constChunkSize];
    qint64 totalBytes = src.size();
    qint64 readPos = 0;
    qint64 bytesRead = 0;
    qint64 adjustTotal = Device::constNoCover!=deviceOpts.coverName ? 16384 : 0;

    do {
        if (stopRequested) {
            emit result(Device::Cancelled);
            return;
        }
        bytesRead = src.read(buffer, constChunkSize);
        readPos+=bytesRead;
        if (bytesRead<0) {
            emit result(Device::ReadFailed);
            return;
        }

        if (stopRequested) {
            emit result(Device::Cancelled);
            return;
        }

        qint64 writePos=0;
        do {
            qint64 bytesWritten = dest.write(&buffer[writePos], bytesRead - writePos);
            if (stopRequested) {
                emit result(Device::Cancelled);
                return;
            }
            if (-1==bytesWritten) {
                emit result(Device::WriteFailed);
                return;
            }
            writePos+=bytesWritten;
        } while (writePos<bytesRead);

        setPercent(((readPos+bytesRead)*100.0)/(totalBytes+adjustTotal));
        if (src.atEnd()) {
            break;
        }
    } while ((readPos+bytesRead)<totalBytes);

    updateTagsDest();
    copyCover(origSrcFile);
    setPercent(100);
    emit result(Device::Ok);
}

void DeleteJob::run()
{
    int status=QFile::remove(fileName) ? Device::Ok : Device::Failed;
    if (remLyrics && Device::Ok==status) {
        QString lyrics=Utils::changeExtension(fileName, SongView::constExtension);
        if (lyrics!=fileName) {
            QFile::remove(lyrics);
        }
    }
    emit result(status);
    emit percent(100);
}

void CleanJob::run()
{
    int total=dirs.count();
    int current=0;
    foreach (const QString &d, dirs) {
        if (stopRequested) {
            emit result(Device::Cancelled);
            return;
        }
        Device::cleanDir(d, base, coverFile);
        emit percent((++current*100)/total);
    }

    emit percent(100);
    emit result(Device::Ok);
}
