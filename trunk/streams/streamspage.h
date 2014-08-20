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

#ifndef STREAMSPAGE_H
#define STREAMSPAGE_H

#include "ui_streamspage.h"
#include "models/streamsproxymodel.h"
#include "models/streamsearchmodel.h"
#include "models/streamsmodel.h"
#include "gui/page.h"

class Action;
class QAction;
class QNetworkReply;

class StreamsPage : public QWidget, public Ui::StreamsPage, public Page
{
    Q_OBJECT

public:
    StreamsPage(QWidget *p);
    virtual ~StreamsPage();

    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    void setView(int v) { view->setMode((ItemView::Mode)v); searchView->setMode((ItemView::Mode)v); }
    void focusSearch() { view->focusSearch(); searchView->focusSearch(); }
    void goBack() { itemView()->backActivated(); }
    ItemView *itemView() { return searching ? searchView : view; }
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void add(const QStringList &streams, bool replace, quint8 priorty);

    void error(const QString &str);
    void showPreferencesPage(const QString &page);

public Q_SLOTS:
    void removeItems();
    void controlActions();

private Q_SLOTS:
    void configureStreams();
    void diSettings();
    void importXml();
    void exportXml();
    void add();
    void addBookmark();
    void addToFavourites();
    void reload();
    void edit();
    void searchItems();
    void controlSearch(bool on);
    void itemDoubleClicked(const QModelIndex &index);
    void updateDiStatus();
    void showPreferencesPage();
    void expandFavourites();
    void addedToFavourites(const QString &name);

private:
    void addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty=0);
    StreamsModel::CategoryItem *getSearchCategory();

private:
    bool searching;
    Action *importAction;
    Action *exportAction;
    Action *addAction;
    Action *editAction;
    StreamsProxyModel streamsProxy;
    StreamsProxyModel searchProxy;
    StreamsProxyModel *proxy;
    StreamSearchModel searchModel;
};

#endif
