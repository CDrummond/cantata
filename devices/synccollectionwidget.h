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

#ifndef _SYNCCOLLECTIONWIDGET_H_
#define _SYNCCOLLECTIONWIDGET_H_

#include "ui_synccollectionwidget.h"
#include "mpd-interface/song.h"
#include "models/musiclibrarymodel.h"
#include "models/musiclibraryproxymodel.h"
#include <QSet>

class QTimer;
class Action;

class SyncCollectionWidget : public QWidget, Ui::SyncCollectionWidget
{
    Q_OBJECT

public:
    SyncCollectionWidget(QWidget *parent, const QString &title);
    ~SyncCollectionWidget() override;

    void clear() { model.clear(); }
    void update(const QSet<Song> &songs);
    void setSupportsAlbumArtistTag(bool s) { model.setSupportsAlbumArtistTag(s); }
    int numArtists() { return model.rowCount(); }
    int numCheckedSongs() const { return checked.count(); }
    QList<Song> checkedSongs() const;

Q_SIGNALS:
    void selectionChanged();
    void configure();

private Q_SLOTS:
    void dataChanged(const QModelIndex &tl, const QModelIndex &br);
    void checkItems();
    void unCheckItems();
    void delaySearchItems();
    void searchItems();
    void expandAll();
    void collapseAll();
    void itemClicked(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);

private:
    void checkItems(bool c);
    void songToggled(const MusicLibraryItemSong *song);

private:
    bool performedSearch;
    MusicLibraryModel model;
    MusicLibraryProxyModel proxy;
    QTimer *searchTimer;
    QSet<const Song *> checked;
    quint64 spaceRequired;
    Action *checkAction;
    Action *unCheckAction;
};

#endif
