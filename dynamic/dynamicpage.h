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

#ifndef DYNAMICPAGE_H
#define DYNAMICPAGE_H

#include "ui_dynamicpage.h"
#include "dynamicproxymodel.h"

class Action;

class DynamicPage : public QWidget, public Ui::DynamicPage
{
    Q_OBJECT

public:
    DynamicPage(QWidget *p);
    virtual ~DynamicPage();

    void focusSearch() { view->focusSearch(); }

public Q_SLOTS:
    void searchItems();
    void controlActions();

private Q_SLOTS:
    void dynamicUrlChanged(const QString &url);
    void add();
    void edit();
    void remove();
    void start();
    void stop();
    void toggle();
    void running(bool status);
    void remoteRunning(bool status);

private:
    void enableWidgets(bool enable);
    void showEvent(QShowEvent *e);
    void hideEvent(QHideEvent *e);

private:
    DynamicProxyModel proxy;
    Action *refreshAction;
    Action *addAction;
    Action *editAction;
    Action *removeAction;
    Action *toggleAction;
};

#endif
