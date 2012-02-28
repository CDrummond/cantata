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
#ifndef DEVICES_MODEL_H
#define DEVICES_MODEL_H

#include <QtCore/QAbstractItemModel>
#include "song.h"
#include "config.h"
#ifdef ENABLE_REMOTE_DEVICES
#include "remotedevice.h"
#endif

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
    #ifdef ENABLE_REMOTE_DEVICES
    void loadRemote();
    void unmountRemote();
    #endif
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool playableOnly=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool playableOnly=false) const;
    void clear();
    QMenu * menu() { return itemMenu; }
    Device * device(const QString &udi);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    QMimeData * mimeData(const QModelIndexList &indexes) const;

public Q_SLOTS:
    void setCover(const QString &artist, const QString &album, const QImage &img, const QString &file);
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void deviceUpdating(const QString &udi, bool state);
    void emitAddToDevice();
    #ifdef ENABLE_REMOTE_DEVICES
    void addRemoteDevice(const QString &path, const QString &coverFileName, const Device::Options &opts, const RemoteDevice::Details &details);
    void removeRemoteDevice(const QString &udi);
    void changeDeviceUdi(const QString &from, const QString &to);
    #endif

private:
    void updateItemMenu();

Q_SIGNALS:
//     void updated(const MusicLibraryItemRoot *root);
    void updateGenres(const QSet<QString> &genres);
    void addToDevice(const QString &udi);
    void error(const QString &text);

private:
    QList<Device *> devices;
    QHash<QString, int> indexes;
    QMenu *itemMenu;
    bool enabled;

    friend class Device;
};

#endif
