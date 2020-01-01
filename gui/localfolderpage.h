/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef LOCAL_FOLDER_PAGE_H
#define LOCAL_FOLDER_PAGE_H

#include "widgets/singlepagewidget.h"
#include "models/localbrowsemodel.h"

class Action;

class LocalFolderBrowsePage : public SinglePageWidget
{
    Q_OBJECT
public:
    LocalFolderBrowsePage(bool isHome, QWidget *p);
    ~LocalFolderBrowsePage() override;

    QString name() const { return model->name(); }
    QString title() const { return model->title(); }
    QString descr() const { return model->descr(); }
    const QIcon & icon() const { return model->icon(); }
    QList<Song> selectedSongs(bool allowPlaylists=false) const override;

private:
    QString configGroup() const;
    void setView(int v) override;
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void controlActions() override;
    void focusSearch() override { }

private Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void headerClicked(int level);
    void openFileManager();

private:
    bool isHomeFolder;
    LocalBrowseModel *model;
    FileSystemProxyModel *proxy;
    Action *browseAction;
};

#endif
