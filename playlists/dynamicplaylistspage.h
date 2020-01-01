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

#ifndef DYNAMIC_RULES_PAGE_H
#define DYNAMIC_RULES_PAGE_H

#include "widgets/singlepagewidget.h"
#include "playlistproxymodel.h"

class Action;
class QLabel;

class DynamicPlaylistsPage : public SinglePageWidget
{
    Q_OBJECT

public:
    DynamicPlaylistsPage(QWidget *p);
    ~DynamicPlaylistsPage() override;
    void setView(int) override { }

private Q_SLOTS:
    void remoteDynamicSupport(bool s);
    void add();
    void edit();
    void remove();
    void start();
    void stop();
    void toggle();
    void running(bool status);
    void headerClicked(int level);

private:
    void doSearch() override;
    void controlActions() override;
    void enableWidgets(bool enable);
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    PlaylistProxyModel proxy;
    Action *addAction;
    Action *editAction;
    Action *removeAction;
    Action *toggleAction;
    QList<QWidget *> controls;
    #ifdef Q_OS_WIN
    QString infoStr;
    QLabel *remoteRunningLabel;
    #endif
    QIcon server;
};

#endif
