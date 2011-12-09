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
#ifndef ALBUMSMODEL_H
#define ALBUMSMODEL_H

#include <QtCore/QAbstractListModel>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtGui/QImage>

class MusicLibraryItemRoot;
class QImage;
class QSize;

class AlbumsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum CoverSize
    {
        CoverSmall  = 0,
        CoverMedium = 1,
        CoverLarge  = 2
    };

    static CoverSize currentCoverSize();
    static void setCoverSize(CoverSize size);
    static int coverPixels();
    static void setItemSize(const QSize &sz);

    enum Columnms
    {
        COL_NAME,
        COL_FILES,
        COL_GENRES
    };

    struct Album
    {
        Album(const QString &ar, const QString &al);
        QString artist;
        QString album;
        QString name;
        QStringList files;
        QSet<QString> genres;
        QImage cover;
        bool updated;
        bool coverRequested;
    };

    AlbumsModel();
    ~AlbumsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void update(const MusicLibraryItemRoot *root);
    void clear();

public Q_SLOTS:
    void setCover(const QString &artist, const QString &album, const QImage &img);

private:
    mutable QList<Album> items;
};

#endif
