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

#ifndef TRANSCODING_JOB_H
#define TRANSCODING_JOB_H

#include "filejob.h"
#include <QtCore/QStringList>
#include <QtCore/QProcess>

class TranscodingJob : public FileJob
{
    Q_OBJECT
public:
    explicit TranscodingJob(const QStringList &params);
    ~TranscodingJob();

    void start();
    void stop();

private Q_SLOTS:
    void processOutput();
    void finished( int exitCode, QProcess::ExitStatus exitStatus );

private:
    inline qint64 computeDuration(const QString &output);
    inline qint64 computeProgress(const QString &output);

private:
    QStringList parameters;
    QProcess *process;
    qint64 duration; //in csec
};


#endif //TRANSCODING_JOB_H
