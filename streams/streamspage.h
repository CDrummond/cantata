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

#include "widgets/singlepagewidget.h"
#include "widgets/stackedpagewidget.h"
#include "models/streamsproxymodel.h"
#include "models/streamsearchmodel.h"
#include "models/streamsmodel.h"
#include "gui/page.h"
#include <QSet>

class Action;
class QAction;
class NetworkReply;
class ServiceStatusLabel;
class StreamsSettings;

class StreamsBrowsePage : public SinglePageWidget
{
    Q_OBJECT

public:
    StreamsBrowsePage(QWidget *p);
    virtual ~StreamsBrowsePage();

    void addSelectionToPlaylist(const QString &name=QString(), bool replace=false, quint8 priorty=0);
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void add(const QStringList &streams, bool replace, quint8 priorty);

    void error(const QString &str);
    void showPreferencesPage(const QString &page);
    void searchForStreams();

public Q_SLOTS:
    void removeItems();
    void controlActions();

private Q_SLOTS:
    void configure();
    void configureDi();
    void diSettings();
    void importXml();
    void exportXml();
    void add();
    void addBookmark();
    void addToFavourites();
    void reload();
    void edit();
    void itemDoubleClicked(const QModelIndex &index);
    void updateDiStatus();
    void expandFavourites();
    void addedToFavourites(const QString &name);
    void tuneInResolved();
    void headerClicked(int level);

private:
    void doSearch();
    void addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty=0);

private:
    ServiceStatusLabel *diStatusLabel;
    Action *importAction;
    Action *exportAction;
    Action *addAction;
    Action *editAction;
    Action *searchAction;
    StreamsProxyModel proxy;
    QSet<NetworkJob *> resolveJobs;
    StreamsSettings *settings;
};

class StreamSearchPage : public SinglePageWidget
{
    Q_OBJECT
public:
    StreamSearchPage(QWidget *p);
    virtual ~StreamSearchPage();
    void showEvent(QShowEvent *e);

private Q_SLOTS:
    void headerClicked(int level);

private:
    void doSearch();

private:
    StreamsProxyModel proxy;
    StreamSearchModel model;
};

class StreamsPage : public StackedPageWidget
{
    Q_OBJECT
public:
    StreamsPage(QWidget *p);
    virtual ~StreamsPage();

private Q_SLOTS:
    void searchForStreams();
    void closeSearch();

private:
    StreamsBrowsePage *browse;
    StreamSearchPage *search;
};

#endif
