/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MPD_LIBRARY_MODEL_H
#define MPD_LIBRARY_MODEL_H

#include "sqllibrarymodel.h"

class MpdLibraryModel : public SqlLibraryModel
{
    Q_OBJECT
public:
    static MpdLibraryModel * self();
    MpdLibraryModel();
    QVariant data(const QModelIndex &index, int role) const override;
    void setUseArtistImages(bool u);
    bool useArtistImages() const { return showArtistImages; }
    void load(Configuration &config) override;
    void save(Configuration &config) override;
    void listSongs();
    void cancelListing();

Q_SIGNALS:
    void songListing(const QList<Song> &songs, double pc);

private Q_SLOTS:
    void listNextChunk();
    void cover(const Song &song, const QImage &img, const QString &file);
    void coverUpdated(const Song &song, const QImage &img, const QString &file);
    void artistImage(const Song &song, const QImage &img, const QString &file);

private:
    bool showArtistImages;
    int listingTotal;
    int listingCurrent;
};

#endif
