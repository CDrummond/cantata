/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef PLAYLISTSPAGE_H
#define PLAYLISTSPAGE_H

#include "ui_playlistspage.h"
#include "playlistsproxymodel.h"
#include "itemview.h"
#include "config.h"

class Action;

class PlaylistTableView : public TableView
{
    Q_OBJECT

public:
    PlaylistTableView(QWidget *p);
    virtual ~PlaylistTableView() { }

    void initHeader();
    void saveHeader();

private Q_SLOTS:
    void showMenu();
    void toggleHeaderItem(bool visible);
    void stretchToggled(bool e);

private:
    QMenu *menu;
};

class PlaylistsPage : public QWidget, public Ui::PlaylistsPage
{
    Q_OBJECT
public:
    PlaylistsPage(QWidget *p);
    virtual ~PlaylistsPage();

    void saveConfig();
    void setStartClosed(bool sc);
    bool isStartClosed();
    void updateRows();
    void refresh();
    void clear();
    //QStringList selectedFiles() const;
    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    void setView(int mode);
    void focusSearch() { view->focusSearch(); }
    void goBack() { view->backActivated(); }
    #ifdef ENABLE_DEVICES_SUPPORT
    QList<Song> selectedSongs() const;
    void addSelectionToDevice(const QString &udi);
    #endif
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void loadPlaylist(const QString &name, bool replace);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name);
    void renamePlaylist(const QString &oldname, const QString &newname);
    void removeFromPlaylist(const QString &name, const QList<quint32> &positions);

    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);

private:
    void addItemsToPlayList(const QModelIndexList &indexes, const QString &name, bool replace, quint8 priorty=0);

public Q_SLOTS:
    void removeItems();
    void controlActions();

private Q_SLOTS:
    void savePlaylist();
    void renamePlaylist();
    void removeDuplicates();
    void itemDoubleClicked(const QModelIndex &index);
    void searchItems();
    void updated(const QModelIndex &index);
    void updateGenres(const QModelIndex &);

private:
    Action *renamePlaylistAction;
    Action *removeDuplicatesAction;
    PlaylistsProxyModel proxy;
};

#endif
