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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "albumsmodel.h"
#include "settings.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "playqueuemodel.h"
#include "song.h"
#include "covers.h"

static AlbumsModel::CoverSize coverSize=AlbumsModel::CoverMedium;
static QPixmap *theDefaultIcon=0;
static QSize itemSize;

int AlbumsModel::coverPixels()
{
    switch (coverSize) {
    default:
    case AlbumsModel::CoverSmall:  return 76;
    case AlbumsModel::CoverMedium: return 100;
    case AlbumsModel::CoverLarge:  return 128;
    }
}

static int stdIconSize()
{
    return 128;
}

AlbumsModel::CoverSize AlbumsModel::currentCoverSize()
{
    return coverSize;
}

void AlbumsModel::setItemSize(const QSize &sz)
{
    itemSize=sz;
}

void AlbumsModel::setCoverSize(AlbumsModel::CoverSize size)
{
    if (size!=coverSize) {
        if (theDefaultIcon) {
            delete theDefaultIcon;
            theDefaultIcon=0;
        }
        coverSize=size;
    }
}

AlbumsModel::Album::Album(const QString &ar, const QString &al)
    : artist(ar)
    , album(al)
    , updated(false)
    , coverRequested(false)
{
    name=album+QLatin1String(" - ")+artist;
}

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

QModelIndex AlbumsModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if(row<items.count())
        return createIndex(row, column, (void *)&items.at(row));

    return QModelIndex();
}

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
            theDefaultIcon = new QPixmap(QIcon::fromTheme("media-optical-audio").pixmap(stdIconSize(), stdIconSize())
                                        .scaled(QSize(coverPixels(), coverPixels()),
                                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    case Qt::ToolTipRole: {
        const Album &al=items[index.row()];
        return 0==al.files.count()
            ? al.name
            :
                #ifdef ENABLE_KDE_SUPPORT
                i18np("%1\n1 Track", "%1\n%2 Tracks", al.name, al.files.count());
                #else
                (al.files.count()>1
                    ? tr("%1\n%2 Tracks").arg(al.name).arg(al.files.count())
                    : tr("%1\n1 Track").arg(al.name));
                #endif
    }
    case Qt::DisplayRole:
        return items.at(index.row()).name;
    case Qt::UserRole:
        return items.at(index.row()).files;
    case Qt::UserRole+1:
        return items.at(index.row()).artist;
    case Qt::UserRole+2:
        return items.at(index.row()).album;
    case Qt::SizeHintRole:
        if (!itemSize.isNull()) {
            return itemSize;
        }
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
    QStringList filenames;

    foreach (QModelIndex index, indexes) {
        if (index.row()<items.count()) {
            filenames << items.at(index.row()).files;
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    return mimeData;
}

void AlbumsModel::update(const MusicLibraryItemRoot *root)
{
    QList<Album>::Iterator it=items.begin();
    QList<Album>::Iterator end=items.end();

    for (; it!=end; ++it) {
        (*it).updated=false;
        (*it).coverRequested=false;
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
                    (*it).files=albumItem->sortedTracks();
                    (*it).genres=albumItem->genres();
                    (*it).updated=true;
                    found=true;
                    break;
                }
            }

            if (!found) {
                Album a(artist, album);
                a.files=albumItem->sortedTracks();
                a.genres=albumItem->genres();
                a.updated=true;
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
        emit updated();
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
            (*it).cover=img.scaled(QSize(coverPixels(), coverPixels()),
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QModelIndex idx=index(row, 0, QModelIndex());
            emit dataChanged(idx, idx);
            return;
        }
    }
}

void AlbumsModel::clear()
{
    beginResetModel();
    items.clear();
    endResetModel();
}
