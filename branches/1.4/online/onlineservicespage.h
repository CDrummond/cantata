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

#ifndef ONLINE_SERVICES_PAGE_H
#define ONLINE_SERVICES_PAGE_H

#include "ui_onlineservicespage.h"
#include "onlineservice.h"
#include "models/musiclibraryproxymodel.h"
#include "gui/page.h"

class Action;
class QAction;

class OnlineServicesPage : public QWidget, public Ui::OnlineServicesPage, public Page
{
    Q_OBJECT

public:
    OnlineServicesPage(QWidget *p);
    virtual ~OnlineServicesPage();

    void setEnabled(bool e);
    void clear();
    QStringList selectedFiles() const;
    QList<Song> selectedSongs(bool allowPlaylists=false) const;
    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0, bool randomAlbums=false);
    void setView(int v) { view->setMode((ItemView::Mode)v); }
    void focusSearch() { view->focusSearch(); }
    void refresh();
    void showEvent(QShowEvent *e);

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void controlSearch(bool on);
    void searchItems();
    void controlActions();
    void configureService();
    void refreshService();
    void updateGenres(const QModelIndex &idx);
    void setSearchable(const QModelIndex &idx);
    void updated(const QModelIndex &idx);
//    void sortList();
    void download();
    void subscribe();
    void unSubscribe();
    void refreshSubscription();
    void downloadPodcast();
    void deleteDownloadedPodcast();
    void showPreferencesPage();
    void providersChanged();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void showPreferencesPage(const QString &page);

private:
    OnlineService * activeSrv() const;
    void expandPodcasts();

private:
    MusicLibraryProxyModel proxy;
    Action *downloadAction;
    Action *downloadPodcastAction;
    Action *deleteDownloadedPodcastAction;
    QSet<QString> genres;
    bool onlineSearchRequest;
    QString searchService;
    bool searchable;
    bool expanded;
};

#endif
