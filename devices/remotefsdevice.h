/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef REMOTEFSDEVICE_H
#define REMOTEFSDEVICE_H

#include "fsdevice.h"
#include "config.h"
#include <QUrl>

class QProcess;
class RemoteFsDevice : public FsDevice
{
    Q_OBJECT

public:
    struct Details
    {
        Details()
            : configured(false) {
        }
        void load(const QString &group=QString());
        void save() const;

        bool operator==(const Details &o) const {
            return url==o.url && extraOptions==o.extraOptions;
        }
        bool operator!=(const Details &o) const {
            return !(*this==o);
        }
        bool isEmpty() const {
            return name.isEmpty() || url.isEmpty();
        }
        bool isLocalFile() const {
            return url.scheme().isEmpty() || constFileProtocol==url.scheme();
        }

        QString name;
        QUrl url;
        QString serviceName;
        QString extraOptions;
        bool configured;
    };

    static const QLatin1String constPromptPassword;
    static const QLatin1String constSshfsProtocol;
    static const QLatin1String constFileProtocol;

    static QList<Device *> loadAll(MusicLibraryModel *m);
    static Device *create(MusicLibraryModel *m, const DeviceOptions &options, const Details &d);
    static void renamed(const QString &oldName, const QString &newName);
    static QString createUdi(const QString &n);

    RemoteFsDevice(MusicLibraryModel *m, const DeviceOptions &options, const Details &d);
    RemoteFsDevice(MusicLibraryModel *m, const Details &d);
    ~RemoteFsDevice() override;

    void toggle() override;
    void mount();
    void unmount();
    bool supportsDisconnect() const override { return !details.isLocalFile(); }
    bool isConnected() const override;
    double usedCapacity() override;
    QString capacityString() override;
    qint64 freeSpace() override;
    void saveOptions() override;
    void configure(QWidget *parent) override;
    DevType devType() const override { return RemoteFs; }
    bool canPlaySongs() const override;
    void destroy(bool removeFromConfig=true);
    const Details & getDetails() const { return details; }
    QString subText() override { return sub; }

Q_SIGNALS:
    void udiChanged();
    void connectionStateHasChanged(const QString &id, bool connected);

protected:
    void load();
    void setup();
    void setAudioFolder() const override;

private:
    bool isOldSshfs();
    QString settingsFileName() const;

protected Q_SLOTS:
    void saveProperties();
    void saveProperties(const DeviceOptions &newOpts, const RemoteFsDevice::Details &newDetails);
    void procFinished(int exitCode);
    void mountStatus(const QString &mp, int pid, int st);
    void umountStatus(const QString &mp, int pid, int st);

protected:
    mutable int mountToken;
    mutable bool currentMountStatus;
    Details details;
    QProcess *proc;
//     QString audioFolderSetting;
    bool messageSent;
    QString sub;
    friend class DevicesModel;
};

#endif
