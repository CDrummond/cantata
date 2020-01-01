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

#ifndef ONLINESEARCH_WIDGET_H
#define ONLINESEARCH_WIDGET_H

#include <QWidget>
#include "widgets/singlepagewidget.h"
#include "onlinesearchservice.h"

class SqueezedTextLabel;

class OnlineSearchWidget : public SinglePageWidget
{
    Q_OBJECT
public:
    OnlineSearchWidget(OnlineSearchService *s, QWidget *p);
    ~OnlineSearchWidget() override;
    QStringList selectedFiles(bool allowPlaylists) const override;
    QList<Song> selectedSongs(bool allowPlaylists) const override;
    void setView(int) override { }
    void showEvent(QShowEvent *e) override;

private Q_SLOTS:
    void headerClicked(int level);
    void statsUpdated(int songs, quint32 time);

private:
    void doSearch() override;

private:
    OnlineSearchService *srv;
    SqueezedTextLabel *statsLabel;
};

#endif
