/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "playlistspage.h"
#include "support/configuration.h"
#include "models/playlistsmodel.h"
#include "dynamic.h"
#include "dynamicpage.h"
#include "storedpage.h"
#include "gui/settings.h"

PlaylistsPage::PlaylistsPage(QWidget *p)
    : MultiPageWidget(p)
{
    stored=new StoredPage(this);
    addPage(PlaylistsModel::self()->name(), PlaylistsModel::self()->icon(), PlaylistsModel::self()->title(), PlaylistsModel::self()->descr(), stored);
    dynamic=new DynamicPage(this);
    addPage(Dynamic::self()->name(), Dynamic::self()->icon(), Dynamic::self()->title(), Dynamic::self()->descr(), dynamic);

    connect(stored, SIGNAL(addToDevice(QString,QString,QList<Song>)), SIGNAL(addToDevice(QString,QString,QList<Song>)));
    Configuration config(metaObject()->className());
    load(config);
}

PlaylistsPage::~PlaylistsPage()
{
    Configuration config(metaObject()->className());
    save(config);
}

#ifdef ENABLE_DEVICES_SUPPORT
void PlaylistsPage::addSelectionToDevice(const QString &udi)
{
    if (stored==currentWidget()) {
        stored->addSelectionToDevice(udi);
    }
}
#endif
