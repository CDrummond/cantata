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

#ifndef REMOTEDEVICE_H
#define REMOTEDEVICE_H

#include "fsdevice.h"
#include <sys/types.h>

class QProcess;
class RemoteDevice : public FsDevice
{
    Q_OBJECT

public:
    enum Protocol
    {
        Prot_None  = 0,
        Prot_Sshfs = 1,
        Prot_File  = 2
    };

    struct Details
    {
        void load(const QString &group);
        void save(const QString &group) const;

        bool operator==(const Details &o) const {
            return protocol==o.protocol && port==o.port && name==o.name && host==o.host && user==o.user && folder==o.folder;
        }
        bool operator!=(const Details &o) const {
            return !(*this==o);
        }
        Details()
            : port(0)
            , protocol(Prot_None) {
        }
        bool isEmpty() const {
            return Prot_None==protocol || name.isEmpty() || folder.isEmpty() || (Prot_Sshfs==protocol && (host.isEmpty() || user.isEmpty() || 0==port));
        }
        QString mountPoint(bool create=false) const;
        QString name;
        QString host;
        QString user;
        QString folder;
        unsigned short port;
        Protocol protocol;
    };

    static QList<Device *> loadAll(DevicesModel *m);
    static RemoteDevice * create(DevicesModel *m, const QString &audio, const QString &cover, const Options &options, const Details &d);
    static void remove(RemoteDevice *dev);
    static QString createUdi(const QString &n);

    RemoteDevice(DevicesModel *m, const QString &audio, const QString &cover, const Options &options, const Details &d);
    RemoteDevice(DevicesModel *m, const Details &d);
    virtual ~RemoteDevice();

    void toggle();
    void mount();
    void unmount();
    bool supportsDisconnect() const { return Prot_File!=details.protocol; }
    bool isConnected() const;
    double usedCapacity();
    QString capacityString();
    qint64 freeSpace();
    void saveOptions();
    void configure(QWidget *parent);
    Type type() const { return Remote; }
    QString udi() const { return createUdi(details.name); }
    QString icon() const {
        return QLatin1String(Prot_File==details.protocol ? "inode-directory" : "network-server");
    }
    virtual bool canPlaySongs() const;

Q_SIGNALS:
    void udiChanged(const QString &from, const QString &to);

protected:
    void load();
    void setup();
    void setAudioFolder();

protected Q_SLOTS:
    void saveProperties();
    void saveProperties(const QString &newPath, const QString &newCoverFileName, const Device::Options &newOpts, const RemoteDevice::Details &newDetails);
    void procFinished(int exitCode);

protected:
    mutable time_t lastCheck;
    mutable bool isMounted;
    Details details;
    QProcess *proc;
    QString audioFolderSetting;
};

#endif
