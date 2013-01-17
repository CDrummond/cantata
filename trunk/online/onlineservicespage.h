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

#ifndef ONLINE_SERVICES_PAGE_H
#define ONLINE_SERVICES_PAGE_H

#include "ui_onlineservicespage.h"
#include "musiclibraryproxymodel.h"
#include "onlineservice.h"

class MainWindow;
class Action;
class QAction;

class OnlineServicesPage : public QWidget, public Ui::OnlineServicesPage
{
    Q_OBJECT

public:
    OnlineServicesPage(MainWindow *p);
    virtual ~OnlineServicesPage();

    void clear();
    QString activeService() const;
    QStringList selectedFiles() const;
    QList<Song> selectedSongs() const;
    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    void setView(bool tree) { view->setMode(tree ? ItemView::Mode_Tree : ItemView::Mode_List); }
    void focusSearch() { view->focusSearch(); }
    void goBack() { view->backActivated(); }

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void searchItems();
    void controlActions();
    void configureService();
    void refreshService();
    void removeService();
    void toggleService();
    void updateGenres(const QModelIndex &);
    void addJamendo();
    void addMagnatune();
    void download();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);

private:
    void addService(const QString &name);
    OnlineService * activeSrv() const;

private:
    MainWindow *mw;
    MusicLibraryProxyModel proxy;
    Action *configureAction;
    Action *refreshAction;
    Action *toggleAction;
    Action *addAction;
    Action *removeAction;
    Action *downloadAction;
    QAction *jamendoAction;
    QAction *magnatuneAction;
    QSet<QString> genres;
};

#endif
