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

#include <QAbstractItemModel>
#include <QSet>
#include "song.h"
#include "config.h"
#include "remotefsdevice.h"

class QMimeData;
class Device;
class QMenu;

class DevicesModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static DevicesModel * self();

    DevicesModel(QObject *parent = 0);
    ~DevicesModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    void clear();
    QMenu * menu() { return itemMenu; }
    Device * device(const QString &udi);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    #ifdef ENABLE_REMOTE_DEVICES
    void unmountRemote();
    #endif
    void toggleGrouping();
    const QSet<QString> & genres() { return devGenres; }

public Q_SLOTS:
    void setCover(const Song &song, const QImage &img);
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void accessibilityChanged(bool accessible, const QString &udi);
    void deviceUpdating(const QString &udi, bool state);
    void emitAddToDevice();
    void addRemoteDevice(const DeviceOptions &opts, RemoteFsDevice::Details details);
    void removeRemoteDevice(const QString &udi);
    void remoteDeviceUdiChanged();
    void removeDeviceConnectionStateChanged(const QString &udi, bool state);
    void mountsChanged();

private:
    void updateItemMenu();
    void addLocalDevice(const QString &udi);
    void loadLocal();
    #ifdef ENABLE_REMOTE_DEVICES
    void loadRemote();
    #endif
    int indexOf(const QString &udi);
    void updateGenres();

Q_SIGNALS:
    void updateGenres(const QSet<QString> &genres);
    void addToDevice(const QString &udi);
    void error(const QString &text);

private:
    QList<Device *> devices;
    QSet<QString> volumes;
    QSet<QString> devGenres;
    QMenu *itemMenu;
    bool enabled;
    bool inhibitMenuUpdate;

    friend class Device;
};

#endif
