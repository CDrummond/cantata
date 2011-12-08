/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "albumsmodel.h"
#include "albumsproxymodel.h"

class MusicLibraryItemRoot;

class AlbumsPage : public QWidget, public Ui::AlbumsPage
{
    Q_OBJECT
public:

    enum Refresh
    {
        RefreshFromCache,
        RefreshForce,
        RefreshStandard
    };

    AlbumsPage(MainWindow *p);
    virtual ~AlbumsPage();

    void refresh(Refresh type);
    void clear();
    void addSelectionToPlaylist();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void add(const QStringList &files);

public Q_SLOTS:
    void updateGenres(const QStringList &genres);
    void itemActivated(const QModelIndex &);
    void searchItems();
    void update(const MusicLibraryItemRoot *root) { model.update(root); }

private:
    AlbumsModel model;
    AlbumsProxyModel proxy;
};

#endif
