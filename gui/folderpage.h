/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "widgets/singlepagewidget.h"
#include "models/browsemodel.h"

class Action;

class FolderPage : public SinglePageWidget
{
    Q_OBJECT
public:
    FolderPage(QWidget *p);
    virtual ~FolderPage();

    void setEnabled(bool e) { model.setEnabled(e); }
    bool isEnabled() const { return model.isEnabled(); }
    void load() { model.load(); }
    void clear() { model.clear(); }
    QStringList selectedFiles(bool allowPlaylists=false) const;
    QList<Song> selectedSongs(bool allowPlaylists=false) const;
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi);
    void deleteSongs();
    #endif
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priorty=0);
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void openFileManager();

private:
    void doSearch() { }
    void controlActions();

private:
    Action *browseAction;
    BrowseModel model;
};

#endif
