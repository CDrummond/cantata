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

#include "mpdsearchmodel.h"
#include "roles.h"
#include "mpd-interface/mpdconnection.h"
#include "gui/covers.h"

MpdSearchModel::MpdSearchModel(QObject *parent)
    : SearchModel(parent)
    , currentId(0)
{
    connect(this, SIGNAL(getRating(QString)), MPDConnection::self(), SLOT(getRating(QString)));
    connect(this, SIGNAL(search(QString,QString,int)), MPDConnection::self(), SLOT(search(QString,QString,int)));
    connect(MPDConnection::self(), SIGNAL(searchResponse(int,QList<Song>)), this, SLOT(searchFinished(int,QList<Song>)));
    connect(MPDConnection::self(), SIGNAL(rating(QString,quint8)), SLOT(ratingResult(QString,quint8)));
    connect(Covers::self(), SIGNAL(loaded(Song,int)), this, SLOT(coverLoaded(Song,int)));
}

MpdSearchModel::~MpdSearchModel()
{
}

QVariant MpdSearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() && Cantata::Role_RatingCol==role) {
        return COL_RATING;
    }

    const Song *song = toSong(index);

    if (!song) {
        return QVariant();
    }

    switch (role) {
    case Cantata::Role_SongWithRating: {
        QVariant var;
        if (Song::Standard==song->type && Song::Rating_Null==song->rating) {
            emit getRating(song->file);
            song->rating=Song::Rating_Requested;
        }
        var.setValue<Song>(*song);
        return var;
    }
    default:
        return SearchModel::data(index, role);
    }
    return QVariant();
}

void MpdSearchModel::clear()
{
    SearchModel::clear();
    currentId++;
}

void MpdSearchModel::search(const QString &key, const QString &value)
{
    if (key==currentKey && value==currentValue) {
        return;
    }
    emit searching();
    clear();
    currentKey=key;
    currentValue=value;
    currentId++;
    emit search(key, value, currentId);
}

void MpdSearchModel::searchFinished(int id, const QList<Song> &result)
{
    if (id!=currentId) {
        return;
    }

    results(result);
}

void MpdSearchModel::coverLoaded(const Song &song, int s)
{
    Q_UNUSED(s)
    if (!song.isArtistImageRequest() && !song.isComposerImageRequest()) {
        int row=0;
        for (const Song &s: songList) {
            if (s.albumArtist()==song.albumArtist() && s.album==song.album) {
                QModelIndex idx=index(row, 0, QModelIndex());
                emit dataChanged(idx, idx);
            }
            row++;
        }
    }
}

void MpdSearchModel::ratingResult(const QString &file, quint8 r)
{
    QList<Song>::iterator it=songList.begin();
    QList<Song>::iterator end=songList.end();
    int numCols=columnCount(QModelIndex())-1;

    for (int row=0; it!=end; ++it, ++row) {
        if (Song::Standard==(*it).type && r!=(*it).rating && (*it).file==file) {
            (*it).rating=r;
            emit dataChanged(index(row, 0), index(row, numCols));
        }
    }
}

#include "moc_mpdsearchmodel.cpp"
