/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QModelIndex>
#include <QtCore/QDataStream>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtGui/QIcon>
#include "albumsmodel.h"
#include "settings.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "song.h"
#include "covers.h"

AlbumsModel::AlbumsModel()
    : QAbstractListModel(0)
{
}

AlbumsModel::~AlbumsModel()
{
}

QVariant AlbumsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int AlbumsModel::rowCount(const QModelIndex &) const
{
    return items.size();
}

static QPixmap *theDefaultIcon=0;

QVariant AlbumsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= items.size()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DecorationRole: {
        AlbumsModel *that=(AlbumsModel *)this;
        Album &al=that->items[index.row()];

        if (!al.cover.isNull()) {
            return al.cover;
        }
        if (!theDefaultIcon) {
            theDefaultIcon = new QPixmap(QIcon::fromTheme("media-optical-audio").pixmap(128, 128)
                                        .scaled(QSize(100, 100), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        if (!al.coverRequested) {
            Song s;
            s.artist=al.artist;
            s.album=al.album;
            if (al.files.count()) {
                s.file=al.files.first();
            }
            Covers::self()->get(s);
            al.coverRequested=true;
        }
        return *theDefaultIcon;
    }
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return items.at(index.row()).name();
    }

    return QVariant();
}

Qt::ItemFlags AlbumsModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::NoItemFlags;
}

QMimeData * AlbumsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QStringList filenames;

    foreach (QModelIndex index, indexes) {
        if (index.row()<items.count()) {
            filenames << items.at(index.row()).files;
        }
    }

    for (int i = filenames.size() - 1; i >= 0; i--) {
        stream << filenames.at(i);
    }

    mimeData->setData("application/cantata_songs_filename_text", encodedData);
    return mimeData;
}

void AlbumsModel::update(const MusicLibraryItemRoot *root)
{
    QList<Album>::Iterator it=items.begin();
    QList<Album>::Iterator end=items.end();

    for (; it!=end; ++it) {
        (*it).updated=false;
    }

    bool changed=false;
    for (int i = 0; i < root->childCount(); i++) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemArtist*>(root->child(i));
        QString artist=artistItem->data(0).toString();
        for (int j = 0; j < artistItem->childCount(); j++) {
            MusicLibraryItemAlbum *albumItem = static_cast<MusicLibraryItemAlbum*>(artistItem->child(j));
            QString album=albumItem->data(0).toString();
            bool found=false;
            it=items.begin();
            end=items.end();
            for (; it!=end; ++it) {
                if ((*it).artist==artist && (*it).album==album) {
                    (*it).update(albumItem->sortedTracks(), albumItem->genres());
                    found=true;
                    break;
                }
            }

            if (!found) {
                Album a(artist, album);
                a.update(albumItem->sortedTracks(), albumItem->genres());
                items.append(a);
                changed=true;
            }
        }
    }

    for (it=items.begin(); it!=items.end();) {
        if (!(*it).updated) {
            changed=true;
            QList<Album>::Iterator cur=it;
            ++it;
            items.erase(cur);
        } else {
            ++it;
        }
    }

    if (changed) {
        beginResetModel();
        endResetModel();
    }
}

void AlbumsModel::setCover(const QString &artist, const QString &album, const QImage &img)
{
    if (img.isNull()) {
        return;
    }

    QList<Album>::Iterator it=items.begin();
    QList<Album>::Iterator end=items.end();

    for (int row=0; it!=end; ++it, ++row) {
        if ((*it).artist==artist && (*it).album==album) {
            (*it).cover=img.scaled(QSize(100, 100), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QModelIndex idx=index(row, 0);
            emit dataChanged(idx, idx);
            return;
        }
    }
}

void AlbumsModel::clear()
{
}

