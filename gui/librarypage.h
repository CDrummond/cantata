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

#ifndef LIBRARYPAGE_H
#define LIBRARYPAGE_H

#include "ui_librarypage.h"
#include "mainwindow.h"
#include "musiclibrarymodel.h"
#include "musiclibraryproxymodel.h"

class LibraryPage : public QWidget, public Ui::LibraryPage
{
    Q_OBJECT
public:

    enum Refresh
    {
        RefreshFromCache,
        RefreshForce,
        RefreshStandard
    };

    LibraryPage(MainWindow *p);
    virtual ~LibraryPage();

    void refresh(Refresh type);
    void clear();
    void addSelectionToPlaylist(const QString &name=QString());
    void setView(bool tree);

    MusicLibraryModel & getModel() { return model; }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void add(const QStringList &files);
    void addSongsToPlaylist(const QString &name, const QStringList &files);
    void listAllInfo(const QDateTime &);

public Q_SLOTS:
    void updateGenres(const QStringList &genres);
    void homeActivated();
    void backActivated();
    void itemClicked(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);
    void itemDoubleClicked(const QModelIndex &);
    void searchItems();

private:
    void setLevel(int level, const MusicLibraryItem *i=0);
    QAbstractItemView * view();

private:
    Action *addToPlaylistAction;
    Action *replacePlaylistAction;
    Action *backAction;
    Action *homeAction;
    int currentLevel;
    bool usingTreeView;
//     QString prevSearch;
    QModelIndex prevTopIndex;
    MusicLibraryModel model;
    MusicLibraryProxyModel proxy;
};

#endif
