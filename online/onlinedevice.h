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
 * Boston, MA 02110-1301, USA.
 */

#ifndef ONLINE_DEVICE_H
#define ONLINE_DEVICE_H

#include "device.h"
#include "song.h"

class QNetworkReply;
class OnlineDevice : public Device
{
    Q_OBJECT

public:
    OnlineDevice() : Device(0, QString(), QString()), lastProg(-1), redirects(0), job(0) { }
    virtual ~OnlineDevice() { }

    bool isConnected() const { return true; }
    void rescan(bool) { }
    bool isRefreshing() const { return false; }
    QString path() const { return QString(); }
    void addSong(const Song&, bool, bool) { }
    void copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover);
    void removeSong(const Song&) { }
    void cleanDirs(const QSet<QString>&) { }
    double usedCapacity() { return 0.0; }
    QString capacityString() { return QString(); }
    qint64 freeSpace() { return 0; }
    DevType devType() const { return RemoteFs; }
    void saveOptions() { }

private Q_SLOTS:
    void downloadFinished();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    int lastProg;
    int redirects;
    bool overWrite;
    QNetworkReply *job;
};

#endif
