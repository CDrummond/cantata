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

#ifndef FOLDERPAGE_H
#define FOLDERPAGE_H

#include "ui_folderpage.h"
#include "dirviewmodel.h"
#include "dirviewproxymodel.h"
#include "config.h"

class Action;

class FolderPage : public QWidget, public Ui::FolderPage
{
    Q_OBJECT
public:

    enum EmptySongMod {
        ES_None,
        ES_FillEmpty,
        ES_GuessTags
    };

    FolderPage(QWidget *p);
    virtual ~FolderPage();

    void setEnabled(bool e);
    bool isEnabled() const { return DirViewModel::self()->isEnabled(); }
    void refresh();
    void clear();
    QStringList selectedFiles(bool allowPlaylists=false) const;
    QList<Song> selectedSongs(EmptySongMod esMod, bool allowPlaylists=false) const;
    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi);
    void deleteSongs();
    #endif
    void setView(int v) { view->setMode((ItemView::Mode)v); }
    void focusSearch() { view->focusSearch(); }
    void goBack() { view->backActivated(); }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);
    void loadFolders();

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

public Q_SLOTS:
    void searchItems();
    void controlActions();
    void itemDoubleClicked(const QModelIndex &);
    void openFileManager();

private:
    QStringList walk(QModelIndex rootItem);

private:
    #ifndef Q_OS_WIN
    Action *browseAction;
    #endif
    DirViewProxyModel proxy;
};

#endif
