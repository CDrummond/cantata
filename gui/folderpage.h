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

#ifndef FOLDERPAGE_H
#define FOLDERPAGE_H

#include "ui_folderpage.h"
#include "mainwindow.h"
#include "dirviewmodel.h"
#include "dirviewproxymodel.h"

class FolderPage : public QWidget, public Ui::FolderPage
{
    Q_OBJECT
public:
    FolderPage(MainWindow *p);
    virtual ~FolderPage();

    void setEnabled(bool e);
    bool isEnabled() const { return enabled; }
    void refresh();
    void clear();
    QStringList selectedFiles() const;
    void addSelectionToPlaylist(const QString &name=QString());
    void setView(bool tree) { view->setMode(tree ? ItemView::Mode_Tree : ItemView::Mode_List); }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void add(const QStringList &files);
    void addSongsToPlaylist(const QString &name, const QStringList &files);
    void listAll();

private Q_SLOTS:
    void searchItems();
    void itemDoubleClicked(const QModelIndex &);

private:
    QStringList walk(QModelIndex rootItem);

private:
    bool enabled;
    DirViewModel model;
    DirViewProxyModel proxy;
};

#endif
