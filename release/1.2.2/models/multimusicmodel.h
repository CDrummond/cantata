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
#ifndef MULTI_MUSIC_MODEL_H
#define MULTI_MUSIC_MODEL_H

#include "musicmodel.h"
#include "song.h"
#include <QList>
#include <QObject>

class MusicLibraryItem;
class MusicLibraryItemRoot;

class MultiMusicModel : public MusicModel
{
    Q_OBJECT

public:
    MultiMusicModel(QObject *parent = 0);
    ~MultiMusicModel();
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;
    int rowCount(const QModelIndex &parent=QModelIndex()) const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    void toggleGrouping();
    QList<Song> songs(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    QStringList filenames(const QModelIndexList &indexes, bool playableOnly=false, bool fullPath=false) const;
    const QSet<QString> & genres() { return colGenres; }
    int row(void *i) const { return collections.indexOf((MusicLibraryItemRoot *)i); }
    void clearImages();

Q_SIGNALS:
    void updateGenres(const QSet<QString> &genres);

protected:    
    int indexOf(const QString &id);
    void updateGenres();

protected:
    QList<MusicLibraryItemRoot *> collections;
    QSet<QString> colGenres;
};

#endif
