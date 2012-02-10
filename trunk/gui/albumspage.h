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

#ifndef ALBUMSPAGE_H
#define ALBUMSPAGE_H

#include "ui_albumspage.h"
#include "mainwindow.h"
#include "albumsproxymodel.h"

class MusicLibraryItemRoot;

class AlbumsPage : public QWidget, public Ui::AlbumsPage
{
    Q_OBJECT
public:
    AlbumsPage(MainWindow *p);
    virtual ~AlbumsPage();

    void clear();
    QStringList selectedFiles() const;
    QList<Song> selectedSongs() const;
    void addSelectionToPlaylist(const QString &name=QString());
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi);
    void deleteSongs();
    #endif
    void setView(int v);
    ItemView::Mode viewMode() const { return view->viewMode(); }

private:
    void setItemSize();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

public Q_SLOTS:
    void updateGenres(const QSet<QString> &g);
    void itemActivated(const QModelIndex &);
    void controlActions();
    void searchItems();

private:
    AlbumsProxyModel proxy;
    QSet<QString> genres;
    MainWindow *mw;
};

#endif
