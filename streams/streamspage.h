/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

struct StreamItem
{
    StreamItem(const QString &u=QString(), const QString &mn=QString()) : url(u), modifiedName(mn)  { }
    QString url;
    QString modifiedName;
};

class StreamsBrowsePage : public SinglePageWidget
{
    Q_OBJECT

public:
    StreamsBrowsePage(QWidget *p);
    ~StreamsBrowsePage() override;

    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void showEvent(QShowEvent *e) override;

Q_SIGNALS:
    void error(const QString &str);
    void showPreferencesPage(const QString &page);
    void searchForStreams();

public Q_SLOTS:
    void removeItems() override;
    void controlActions() override;
    void addToFavourites(const QList<StreamItem> &items);

private Q_SLOTS:
    void configure();
    void configureDi();
    void diSettings();
    void importXml();
    void exportXml();
    void addStream();
    void addBookmark();
    void reload();
    void edit();
    void itemDoubleClicked(const QModelIndex &index);
    void updateDiStatus();
    void expandFavourites();
    void addedToFavourites(const QString &name);
    void tuneInResolved();
    void headerClicked(int level);

private:
    void doSearch() override;
    void addItemsToPlayQueue(const QModelIndexList &indexes, int action, quint8 priority=0, bool decreasePriority=false);
    void addToFavourites();

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
    friend class StreamsPage;
};

class StreamSearchPage : public SinglePageWidget
{
    Q_OBJECT
public:
    StreamSearchPage(QWidget *p);
    ~StreamSearchPage() override;
    void showEvent(QShowEvent *e) override;

Q_SIGNALS:
    void addToFavourites(const QList<StreamItem> &items);

private Q_SLOTS:
    void headerClicked(int level);
    void addedToFavourites(const QString &name);

private:
    void doSearch() override;
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void addToFavourites();

private:
    StreamsProxyModel proxy;
    StreamSearchModel model;
    friend class StreamsPage;
};

class StreamsPage : public StackedPageWidget
{
    Q_OBJECT
public:
    StreamsPage(QWidget *p);
    ~StreamsPage() override;

private Q_SLOTS:
    void searchForStreams();
    void closeSearch();
    void addToFavourites();

private:
    StreamsBrowsePage *browse;
    StreamSearchPage *search;
};

#endif
