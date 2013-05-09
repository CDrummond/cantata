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
 */

#include "transcodingjob.h"
#include "device.h"
#include <QStringList>

TranscodingJob::TranscodingJob(const Encoders::Encoder &enc, int val, const QString &src, const QString &dest, const DeviceOptions &d, int co, const Song &s)
    : CopyJob(src, dest, d, co, s)
    , encoder(enc)
    , value(val)
    , process(0)
    , duration(-1)
{
}

TranscodingJob::~TranscodingJob()
{
    delete process;
}

void TranscodingJob::run()
{
    QString src(updateTagsLocal());
    if (src.isEmpty()) {
        return;
    }

    if (stopRequested) {
        emit result(Device::Cancelled);
    } else {
        QStringList parameters=encoder.params(value, src, destFile);
        process = new QProcess;
        process->setReadChannelMode(QProcess::MergedChannels);
        process->setReadChannel(QProcess::StandardOutput);
        connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(processOutput()));
        connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(finished(int, QProcess::ExitStatus)));
        QString cmd=parameters.takeFirst();
        process->start(cmd, parameters);
    }
}

void TranscodingJob::stop()
{
    if (process) {
        process->close();
        process->deleteLater();
        process=0;
        emit result(Device::Cancelled);
    }
}

void TranscodingJob::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (!process) {
        return;
    }
    if (stopRequested) {
        emit result(Device::Cancelled);
        return;
    }
    if (0==exitCode) {
        updateTagsDest();
        copyCover(srcFile);
    }
    emit result(0==exitCode ? Device::Ok : Device::TranscodeFailed);
}

void TranscodingJob::processOutput()
{
    if (stopRequested) {
        emit result(Device::Cancelled);
        return;
    }
    QString output = process->readAllStandardOutput().data();
    if(output.simplified().isEmpty()) {
        return;
    }

    if (!data.isEmpty()) {
        output=data+output;
    }
    if (-1==duration) {
        duration = computeDuration(output);
    }

    if (duration>0) {
        qint64 prog = computeProgress(output);
        if (prog>-1) {
            setPercent((prog*100)/duration);
        }
    }

    if (!output.endsWith('\n') && !output.endsWith('\r')) {
        int last=output.lastIndexOf('\n');
        if (-1==last) {
            last=output.lastIndexOf('\r');
        }
        if (last>-1) {
            data=output.mid(last+1);
        } else {
            data=output;
        }
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
