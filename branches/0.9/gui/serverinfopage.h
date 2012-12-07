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

#ifndef SERVERINFOPAGE_H
#define SERVERINFOPAGE_H

#include <QtGui/QWidget>
#include <QtCore/QDateTime>
#include "ui_serverinfopage.h"

class Action;
class Action;
class MPDStats;

class ServerInfoPage : public QWidget, public Ui::ServerInfoPage
{
  Q_OBJECT

public:
    ServerInfoPage(QWidget *p);
    ~ServerInfoPage();

    void clear();

private Q_SLOTS:
    void statsUpdated();
    void mpdVersion(long v);
    void urlHandlers(const QStringList &handlers);

private:
    Action *updateAction;
};

#endif
