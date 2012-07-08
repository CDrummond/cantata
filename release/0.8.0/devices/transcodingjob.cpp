/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 */
/****************************************************************************************
 * Copyright (c) 2010 Téo Mrnjavac <teo@kde.org>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "transcodingjob.h"

TranscodingJob::TranscodingJob(const QStringList &params, QObject *parent)
    : KJob(parent)
    , duration(-1)
{
    QStringList p(params);
    QString cmd=p.takeFirst();

    process = new KProcess(this);
    process->setOutputChannelMode(KProcess::MergedChannels);
    process->setProgram(cmd);
    *process << p;

    connect(process, SIGNAL(readyRead()), this, SLOT(processOutput()));
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(finished(int, QProcess::ExitStatus)));
}

void TranscodingJob::start()
{
    process->start();
}

void TranscodingJob::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    emitResult();
}

void TranscodingJob::processOutput()
{
    QString output = process->readAllStandardOutput().data();
    if(output.simplified().isEmpty()) {
        return;
    }

    if (-1==duration) {
        duration = computeDuration(output);
        if(duration >= 0) {
            setTotalAmount(KJob::Bytes, duration);
        }
    }

    qint64 progress = computeProgress(output);
    if(progress > -1) {
        setProcessedAmount(KJob::Bytes, progress);
    }
}

inline qint64 TranscodingJob::computeDuration(const QString &output)
{
    //We match something like "Duration: 00:04:33.60"
    QRegExp matchDuration("Duration: (\\d{2,}):(\\d{2}):(\\d{2})\\.(\\d{2})");

    if(output.contains(matchDuration)) {
        //duration is in csec
        return matchDuration.cap(1).toLong() * 60 * 60 * 100 +
               matchDuration.cap(2).toInt()  * 60 * 100 +
               matchDuration.cap(3).toInt()  * 100 +
               matchDuration.cap(4).toInt();
    } else {
        return -1;
    }
}

inline qint64 TranscodingJob::computeProgress(const QString &output)
{
    //Output is like size=     323kB time=18.10 bitrate= 146.0kbits/s
    //We're going to use the "time" column, which counts the elapsed time in seconds.
    QRegExp matchTime("time=(\\d+)\\.(\\d{2})");

    if(output.contains(matchTime)) {
        return matchTime.cap(1).toLong() * 100 +
               matchTime.cap(2).toInt();
    } else {
        return -1;
    }
}
