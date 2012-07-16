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

#ifndef _SYNCCOLLECTIONWIDGET_H_
#define _SYNCCOLLECTIONWIDGET_H_

#include "ui_synccollectionwidget.h"
#include "song.h"
#include <QtCore/QSet>

class MusicLibraryModel;
class MusicLibraryProxyModel;

class SyncCollectionWidget : public QWidget, Ui::SyncCollectionWidget
{
    Q_OBJECT

public:
    SyncCollectionWidget(QWidget *parent, const QString &title, const QString &action);
    virtual ~SyncCollectionWidget();

    void setIcon(const QString &iconName);
    void update(const QSet<Song> &songs);
    int numArtists();

Q_SIGNALS:
    void copy(const QList<Song> &songs);

private Q_SLOTS:
    void copySongs();

private:
    MusicLibraryModel *model;
    MusicLibraryProxyModel *proxy;
};

#endif
