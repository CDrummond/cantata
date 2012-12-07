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

#ifndef FILE_JOB_H
#define FILE_JOB_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include "song.h"
#include "deviceoptions.h"

class QTemporaryFile;
class QThread;
class FileJob;

class FileScheduler : public QObject
{
    Q_OBJECT
public:
    static FileScheduler * self();

    FileScheduler();
    void addJob(FileJob *job);
    void stop();
private:
    QThread *thread;
};

class FileJob : public QObject
{
    Q_OBJECT

public:
    enum Status {
        StatusOk,
        StatusFailed,
        StatusCancelled
    };

    static void finished(QObject *obj) {
        if (obj) {
            obj->deleteLater();
        }
    }

    FileJob();
    virtual ~FileJob() { }

    void setPercent(int pc);
    bool wasStarted() const { return 0!=progressPercent && 100!=progressPercent; }

Q_SIGNALS:
    void percent(int pc);
    void result(int status);

public:
    virtual void stop() { stopRequested=true; }
    virtual void start();

protected Q_SLOTS:
    virtual void run()=0;

protected:
    bool stopRequested;
    int progressPercent;
};

class CopyJob : public FileJob
{
    Q_OBJECT

public:
    enum Options
    {
        OptsNone         = 0x00,
        OptsApplyVaFix   = 0x01,
        OptsUnApplyVaFix = 0x02,
        OptsFixLocal     = 0x04  // Apply any fixes to a local temp file before sending...
    };

    CopyJob(const QString &src, const QString &dest, const DeviceOptions &d, int co, const Song &s)
        : srcFile(src)
        , destFile(dest)
        , deviceOpts(d)
        , copyOpts(co)
        , song(s)
        , temp(0)
        , copiedCover(false) {
    }
    virtual ~CopyJob();

    bool coverCopied() const { return copiedCover; }

protected:
    QString updateTagsLocal();
    void updateTagsDest();
    void copyCover(const QString &origSrcFile);

private:
    virtual void run();

protected:
    QString srcFile;
    QString destFile;
    DeviceOptions deviceOpts;
    int copyOpts;
    Song song;
    QTemporaryFile *temp;
    bool copiedCover;
};

class DeleteJob : public FileJob
{
public:
    DeleteJob(const QString &file, bool rl=false)
        : fileName(file)
        , remLyrics(rl) {
    }
private:
    void run();
private:
    QString fileName;
    bool remLyrics;
};

class CleanJob : public FileJob
{
public:
    CleanJob(const QSet<QString> &d, const QString &b, const QString &cf)
        : dirs(d)
        , base(b)
        , coverFile(cf) {
    }
private:
    void run();
private:
    QSet<QString> dirs;
    QString base;
    QString coverFile;
};

#endif
