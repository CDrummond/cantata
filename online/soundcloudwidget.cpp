/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "soundcloudwidget.h"
#include "widgets/itemview.h"
#include "support/action.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include <QTimer>

SoundCloudWidget::SoundCloudWidget(SoundCloudService *s, QWidget *p)
    : SinglePageWidget(p)
    , srv(s)
{
    view->setModel(s);
    init(ReplacePlayQueue|AddToPlayQueue);
    view->alwaysShowHeader();
    view->setPermanentSearch();
    view->setMode(ItemView::Mode_List);
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
}

SoundCloudWidget::~SoundCloudWidget()
{
}

QStringList SoundCloudWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(selected, allowPlaylists);
}

QList<Song> SoundCloudWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(selected, allowPlaylists);
}

void SoundCloudWidget::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void SoundCloudWidget::doSearch()
{
    srv->search(QString(), view->searchText());
}
