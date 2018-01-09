/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "devices/device.h"
#include "mpd-interface/song.h"

class NetworkJob;
class OnlineDevice : public Device
{
    Q_OBJECT

public:
    OnlineDevice() : Device(nullptr, QString(), QString()), lastProg(-1), overWrite(false), job(nullptr) { }
    ~OnlineDevice() override { }

    bool isConnected() const override { return true; }
    void rescan(bool) override { }
    bool isRefreshing() const override { return false; }
    void stop() override { }
    QString path() const override { return QString(); }
    void addSong(const Song&, bool, bool) override { }
    void copySongTo(const Song &s, const QString &musicPath, bool overwrite, bool copyCover) override;
    void removeSong(const Song&) override { }
    void cleanDirs(const QSet<QString>&) override { }
    double usedCapacity() override { return 0.0; }
    QString capacityString() override { return QString(); }
    qint64 freeSpace() override { return 0; }
    DevType devType() const override { return RemoteFs; }
    void saveOptions() override { }

private Q_SLOTS:
    void downloadFinished();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    int lastProg;
    bool overWrite;
    NetworkJob *job;
};

#endif
