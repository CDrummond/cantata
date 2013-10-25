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
 * along with this program; see the file COPYING.  If not, readStatusite to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "tagclient.h"
#include "tags.h"
#include "config.h"
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QDataStream>
#include <QApplication>
#include <signal.h>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "TagClient" << __FUNCTION__

void TagClient::enableDebug()
{
    debugEnabled=true;
}

static void stopHelper();

static void signalHandler(int signum)
{
    stopHelper();
    exit(signum);
}

static struct TagClientClose { ~TagClientClose() { stopHelper(); } } closer;

static const int constMaxWait=5000;
static QMutex mutex;
static QProcess *proc=0;

static void init()
{
    static bool initialised=false;
    if (!initialised) {
        // Need to stop helper if Cantata crashes...
        QList<int> sig=QList<int>() << SIGINT << SIGILL << SIGTRAP << SIGTRAP << SIGABRT << SIGBUS << SIGFPE << SIGSEGV << SIGTERM;
        foreach (int s, sig) {
            signal(s, signalHandler);
        }
        initialised=true;
    }
}

enum ReadStatus {
    Read_Ok,
    Read_Timeout,
    Read_Closed,
    Read_Error
};

static bool running()
{
    return proc && QProcess::Running==proc->state() && proc->isOpen();
}

static void stopHelper()
{
    if (!proc) {
        return;
    }
    DBUG << (void *)proc;
    QProcess *p=proc;

    proc=0;
    if (p) {
        p->terminate();
        p->kill();
        p->deleteLater();
    }
}

static bool startHelper()
{
    DBUG << (void *)proc;
    if (!running()) {
        init();
        stopHelper();
        for (int i=0; i<5; ++i) { // Max5 start attempts...
            DBUG << "start process";
            proc=new QProcess;
            proc->setProcessChannelMode(QProcess::SeparateChannels);
            proc->setReadChannel(QProcess::StandardOutput);
            #ifdef Q_OS_WIN
            proc->start(qApp->applicationDirPath()+"/cantata-tags.exe");
            #else
            proc->start(INSTALL_PREFIX"/lib/cantata/cantata-tags");
            #endif
            if (proc->waitForStarted(constMaxWait)) {
                DBUG << "started";
                return true;
            } else {
                DBUG << "failed to start";
                stopHelper();
            }
        }
        return false;
    }
    return true;
}

static ReadStatus readReply(QByteArray &data)
{
    DBUG << (void *)proc;
    if (!running()) {
        DBUG << "not running?";
        stopHelper();
        return Read_Closed;
    }
    if (proc->waitForReadyRead(constMaxWait)) {
        data=proc->readAllStandardOutput();
        DBUG << "read reply, bytes:" << data.length();
        return data.isEmpty() ? Read_Error : Read_Ok;
    }
    DBUG << "wait for read failed " << running() << (proc ? (int)proc->state() : 12345);
    stopHelper();
    return !running() ? Read_Closed : Read_Timeout;
}

Song TagClient::read(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    Song resp;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName;
        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

QImage TagClient::readImage(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    QImage resp;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName;
        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

QString TagClient::readLyrics(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    QString resp;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName;
        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

int TagClient::updateArtistAndTitle(const QString &fileName, const Song &song)
{
    QMutexLocker locker(&mutex);
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName << song;
        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

int TagClient::update(const QString &fileName, const Song &from, const Song &to, int id3Ver)
{
    QMutexLocker locker(&mutex);
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName << from << to << id3Ver;
        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

Tags::ReplayGain TagClient::readReplaygain(const QString &fileName)
{
    QMutexLocker locker(&mutex);
    Tags::ReplayGain resp;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName;
        QByteArray data;
        if (Read_Ok==readReply(data)) {
            QDataStream inStream(data);
            inStream >> resp;
        }
    }
    return resp;
}

int TagClient::updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg)
{
    QMutexLocker locker(&mutex);
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName << rg;
        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}

int TagClient::embedImage(const QString &fileName, const QByteArray &cover)
{
    QMutexLocker locker(&mutex);
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream outStream(proc);
        DBUG << __FUNCTION__ << fileName;
        outStream << QString(__FUNCTION__) << fileName << cover;
        QByteArray data;
        ReadStatus readStatus=readReply(data);
        if (Read_Ok==readStatus) {
            QDataStream inStream(data);
            inStream >> resp;
        } else {
            resp=Read_Timeout==readStatus ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return resp;
}
