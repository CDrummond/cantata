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

#include "stackedpagewidget.h"
#include "support/icon.h"
#include "support/utils.h"
#include "support/squeezedtextlabel.h"
#include "listview.h"
#include "sizewidget.h"
#include "singlepagewidget.h"
#include <QGridLayout>
#include <QToolButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QScrollArea>

StackedPageWidget::StackedPageWidget(QWidget *p)
    : QStackedWidget(p)
{
}

StackedPageWidget::~StackedPageWidget()
{
}

void StackedPageWidget::setView(int v)
{
    for (int i=0; i<count(); ++i) {
        if (dynamic_cast<SinglePageWidget *>(widget(i))) {
            static_cast<SinglePageWidget *>(widget(i))->setView(v);
        }
    }
}

void StackedPageWidget::focusSearch()
{
    QWidget *w=currentWidget();
    if (dynamic_cast<SinglePageWidget *>(w)) {
        static_cast<SinglePageWidget *>(w)->focusSearch();
    }
}

QStringList StackedPageWidget::selectedFiles(bool allowPlaylists) const
{
    if (!dynamic_cast<SinglePageWidget *>(currentWidget())) {
        return QStringList();
    }
    return static_cast<SinglePageWidget *>(currentWidget())->selectedFiles(allowPlaylists);
}

QList<Song> StackedPageWidget::selectedSongs(bool allowPlaylists) const
{
    if (!dynamic_cast<SinglePageWidget *>(currentWidget())) {
        return QList<Song>();
    }
    return static_cast<SinglePageWidget *>(currentWidget())->selectedSongs(allowPlaylists);
}

void StackedPageWidget::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    if (!dynamic_cast<SinglePageWidget *>(currentWidget())) {
        return;
    }
    return static_cast<SinglePageWidget *>(currentWidget())->addSelectionToPlaylist(name, replace, priorty);
}
