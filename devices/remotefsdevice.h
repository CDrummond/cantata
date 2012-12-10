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

#ifndef REMOTEFSDEVICE_H
#define REMOTEFSDEVICE_H

#include "fsdevice.h"
#include "config.h"
#include <QtCore/QUrl>

class ComGooglecodeCantataMounterInterface;
class QProcess;
class RemoteFsDevice : public FsDevice
{
    Q_OBJECT

public:
    struct Details
    {
        Details()
            : port(0) {
        }
        void load(const QString &group=QString());
        void save() const;

        bool operator==(const Details &o) const {
            return url==o.url;
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
        int port;
    };

    static const QLatin1String constSshfsProtocol;
    static const QLatin1String constFileProtocol;
    #ifdef ENABLE_MOUNTER
    static const QLatin1String constDomainQuery;
    static const QLatin1String constSambaProtocol;
    #endif

    static QList<Device *> loadAll(DevicesModel *m);
    static Device * create(DevicesModel *m, const DeviceOptions &options, const Details &d);
    static void remove(Device *dev);
    static void renamed(const QString &oldName, const QString &newName);
    static QString createUdi(const QString &n);

    RemoteFsDevice(DevicesModel *m, const DeviceOptions &options, const Details &d);
    RemoteFsDevice(DevicesModel *m, const Details &d);
    virtual ~RemoteFsDevice();

    void toggle();
    void mount();
    void unmount();
    bool supportsDisconnect() const { return !details.isLocalFile(); }
    bool isConnected() const;
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();
    void saveOptions();
    void configure(QWidget *parent);
    DevType devType() const { return RemoteFs; }
    QString icon() const {
        return QLatin1String(details.isLocalFile() ? "inode-directory" : "network-server");
    }
    bool canPlaySongs() const;

Q_SIGNALS:
    void udiChanged();

protected:
    void load();
    void setup();
    void setAudioFolder() const;

private:
    bool isOldSshfs();
    #ifdef ENABLE_MOUNTER
    ComGooglecodeCantataMounterInterface * mounter();
    #endif

protected Q_SLOTS:
    void saveProperties();
    void saveProperties(const DeviceOptions &newOpts, const RemoteFsDevice::Details &newDetails);
    void procFinished(int exitCode);
    void mountStatus(const QString &mp, int st);
    void umountStatus(const QString &mp, int st);

protected:
    mutable int mountToken;
    mutable bool currentMountStatus;
    Details details;
    QProcess *proc;
//     QString audioFolderSetting;
    #ifdef ENABLE_MOUNTER
    ComGooglecodeCantataMounterInterface *mounterIface;
    #endif
    friend class DevicesModel;
};

#endif
