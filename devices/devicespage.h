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

#ifndef DEVICESPAGE_H
#define DEVICESPAGE_H

#include "ui_devicespage.h"
#include "musiclibraryproxymodel.h"
#include "device.h"
#include "config.h"
#ifdef ENABLE_REMOTE_DEVICES
#include "remotefsdevice.h"
#endif
#include "cddb.h"

class Action;

class DevicesPage : public QWidget, public Ui::DevicesPage
{
    Q_OBJECT

public:
    DevicesPage(QWidget *p);
    virtual ~DevicesPage();

    void clear();
    QString activeFsDeviceUdi() const;
    QStringList playableUrls() const;
    QList<Song> selectedSongs() const;
    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    void setView(int v) { view->setMode((ItemView::Mode)v); }
    void focusSearch() { view->focusSearch(); }
    void goBack() { view->backActivated(); }

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void searchItems();
    void controlActions();
    void copyToLibrary();
    void configureDevice();
    void refreshDevice();
    void deleteSongs();
    void addRemoteDevice();
    void forgetRemoteDevice();
    void toggleDevice();
    void sync();
    void updateGenres(const QModelIndex &);
    void updated(const QModelIndex &idx);
    void cddbMatches(const QString &udi, const QList<CddbAlbum> &albums);
    void editDetails();

private:
    Device * activeFsDevice() const;

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

private:
    MusicLibraryProxyModel proxy;
    Action *copyAction;
    #ifdef ENABLE_REMOTE_DEVICES
    Action *forgetDeviceAction;
    #endif
    Action *syncAction;
    QSet<QString> genres;
};

#endif
