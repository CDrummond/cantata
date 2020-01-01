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
#ifndef DEVICES_MODEL_H
#define DEVICES_MODEL_H

#include <QSet>
#include "mpd-interface/song.h"
#include "config.h"
#include "devices/remotefsdevice.h"
#include "musiclibrarymodel.h"
#include "devices/cdalbum.h"
#include "musiclibraryproxymodel.h"

class QMimeData;
class Device;
class MirrorMenu;

class DevicesModel : public MusicLibraryModel
{
    Q_OBJECT

public:
    static DevicesModel * self();

    static void enableDebug();
    static bool debugEnabled();
    static QString fixDevicePath(const QString &path);

    DevicesModel(QObject *parent = nullptr);
    ~DevicesModel() override;
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant headerData(int, Qt::Orientation, int = Qt::DisplayRole) const override { return QVariant(); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 1; }
    int rowCount(const QModelIndex &parent=QModelIndex()) const override;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    QList<Song> songs(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    QStringList filenames(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    int row(void *i) const override { return collections.indexOf(static_cast<MusicLibraryItemRoot *>(i)); }
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList playableUrls(const QModelIndexList &indexes) const;
    void clear(bool clearConfig=true);
    MirrorMenu * menu() { return itemMenu; }
    Device * device(const QString &udi);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    void stop();
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
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

private:
    int indexOf(const QString &id);

public Q_SLOTS:
    void setCover(const Song &song, const QImage &img, const QString &file);
    void setCover(const Song &song, const QImage &img);
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void accessibilityChanged(bool accessible, const QString &udi);
    void deviceUpdating(const QString &udi, bool state);
    void emitAddToDevice();
    void addRemoteDevice(const DeviceOptions &opts, RemoteFsDevice::Details details);
    void removeRemoteDevice(const QString &udi, bool removeFromConfig=true);
    void remoteDeviceUdiChanged();
    void mountsChanged();

private:
    void addLocalDevice(const QString &udi);
    #ifdef ENABLE_REMOTE_DEVICES
    void loadRemote();
    #endif

Q_SIGNALS:
    void addToDevice(const QString &udi);
    void error(const QString &text);
    void updated(const QModelIndex &idx);
    void matches(const QString &udi, const QList<CdAlbum> &albums);
    void invalid(const QList<Song> &songs);
    void updatedDetails(const QList<Song> &songs);
    void add(const QStringList &files, int action, quint8 priority, bool decreasePriority); // add songs to MPD playqueue

private Q_SLOTS:
    void play(const QList<Song> &songs);
    void loadLocal();
    void updateItemMenu();

private:
    QList<MusicLibraryItemRoot *> collections;
    QSet<QString> volumes;
    MirrorMenu *itemMenu;
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
