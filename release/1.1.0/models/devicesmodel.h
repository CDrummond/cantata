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
#ifndef DEVICES_MODEL_H
#define DEVICES_MODEL_H

#include <QSet>
#include "song.h"
#include "config.h"
#include "remotefsdevice.h"
#include "multimusicmodel.h"
#include "cdalbum.h"
#include "musiclibraryproxymodel.h"

class QMimeData;
class Device;
class QMenu;

class DevicesModel : public MultiMusicModel
{
    Q_OBJECT

public:
    static DevicesModel * self();

    static QString fixDevicePath(const QString &path);

    DevicesModel(QObject *parent = 0);
    ~DevicesModel();
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList playableUrls(const QModelIndexList &indexes) const;
    void clear(bool clearConfig=true);
    QMenu * menu() { return itemMenu; }
    Device * device(const QString &udi);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    void stop();
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    #ifdef ENABLE_REMOTE_DEVICES
    void unmountRemote();
    #endif

    Action * configureAct() const { return configureAction; }
    Action * refreshAct() const { return refreshAction; }
    Action * connectAct() const { return connectAction; }
    Action * disconnectAct() const { return disconnectAction; }
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    Action * editAct() const { return editAction; }
    void playCd(const QString &dev);
    #endif

public Q_SLOTS:
    void setCover(const Song &song, const QImage &img);
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void accessibilityChanged(bool accessible, const QString &udi);
    void deviceUpdating(const QString &udi, bool state);
    void emitAddToDevice();
    void addRemoteDevice(const DeviceOptions &opts, RemoteFsDevice::Details details);
    void removeRemoteDevice(const QString &udi, bool removeFromConfig=true);
    void remoteDeviceUdiChanged();
    void removeDeviceConnectionStateChanged(const QString &udi, bool state);
    void mountsChanged();

private:
    void updateItemMenu();
    void addLocalDevice(const QString &udi);
    #ifdef ENABLE_REMOTE_DEVICES
    void loadRemote();
    #endif

Q_SIGNALS:
    void updateGenres(const QSet<QString> &genres);
    void addToDevice(const QString &udi);
    void error(const QString &text);
    void updated(const QModelIndex &idx);
    void matches(const QString &udi, const QList<CdAlbum> &albums);
    void invalid(const QList<Song> &songs);
    void updatedDetails(const QList<Song> &songs);
    void add(const QStringList &files, bool replace, quint8 priorty); // add songs to MPD playqueue

private Q_SLOTS:
    void play(const QList<Song> &songs);
    void loadLocal();

private:
    QSet<QString> volumes;
    QMenu *itemMenu;
    bool enabled;
    bool inhibitMenuUpdate;
    Action *configureAction;
    Action *refreshAction;
    Action *connectAction;
    Action *disconnectAction;
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    QString autoplayCd;
    Action *editAction;
    #endif
    friend class Device;
};

#endif
