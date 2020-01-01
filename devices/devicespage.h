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

#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include "device.h"
#include "widgets/singlepagewidget.h"
#ifdef ENABLE_REMOTE_DEVICES
#include "remotefsdevice.h"
#endif
#include "cdalbum.h"
#include "models/musiclibraryproxymodel.h"

class Action;

class DevicesPage : public SinglePageWidget
{
    Q_OBJECT

public:
    DevicesPage(QWidget *p);
    ~DevicesPage() override;

    void clear();
    QString activeFsDeviceUdi() const;
    QStringList playableUrls() const;
    QList<Song> selectedSongs(bool allowPlaylists=false) const override;
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void focusSearch() override { view->focusSearch(); }
    void refresh() override;
    void resort() { proxy.sort(); }

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void searchItems();
    void controlActions() override;
    void copyToLibrary();
    void configureDevice();
    void refreshDevice();
    void deleteSongs() override;
    void addRemoteDevice();
    void forgetRemoteDevice();
    void sync();
    void toggleDevice();
    void updated(const QModelIndex &idx);
    void cdMatches(const QString &udi, const QList<CdAlbum> &albums);
    void editDetails();

private:
    Device * activeFsDevice() const;

Q_SIGNALS:
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

private:
    MusicLibraryProxyModel proxy;
    Action *copyAction;
    Action *syncAction;
    #ifdef ENABLE_REMOTE_DEVICES
    Action *forgetDeviceAction;
    #endif
};

#endif
