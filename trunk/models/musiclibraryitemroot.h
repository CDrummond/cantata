/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUSIC_LIBRARY_ITEM_ROOT_H
#define MUSIC_LIBRARY_ITEM_ROOT_H

#include <QList>
#include <QVariant>
#include <QHash>
#include <QSet>
#include <QDateTime>
#include "musiclibraryitem.h"
#include "song.h"

class QDateTime;
class QXmlStreamReader;
class QXmlStreamWriter;
class MusicLibraryItemArtist;

class MusicLibraryProgressMonitor
{
public:
    virtual ~MusicLibraryProgressMonitor() { }
    virtual void readProgress(double pc) =0;
    virtual void writeProgress(double pc) =0;
    virtual bool wasStopped() const =0;
};

class MusicLibraryItemRoot : public MusicLibraryItemContainer
{
public:
    MusicLibraryItemRoot(const QString &name=QString(), bool albumArtistSupport=true)
        : MusicLibraryItemContainer(name, 0)
        , supportsAlbumArtist(albumArtistSupport)
        , albumImages(true)
        , artistImages(false)
        , largeImages(false) {
    }
    virtual ~MusicLibraryItemRoot() {
    }

    virtual QString icon() const { return QString(); }
    virtual bool isDevice() const { return false; }
    virtual bool isService() const { return false; }
    MusicLibraryItemArtist * artist(const Song &s, bool create=true);
    MusicLibraryItemArtist * createArtist(const Song &s);
    void groupSingleTracks();
    void groupMultipleArtists();
    bool isFromSingleTracks(const Song &s) const;
    void refreshIndexes();
    void remove(MusicLibraryItemArtist *artist);
    QSet<Song> allSongs(bool revertVa=false) const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    void updateSongFile(const Song &from, const Song &to);
    void toXML(const QString &filename, const QDateTime &date=QDateTime(), MusicLibraryProgressMonitor *prog=0) const;
    void toXML(QXmlStreamWriter &writer, const QDateTime &date=QDateTime(), MusicLibraryProgressMonitor *prog=0) const;
    quint32 fromXML(const QString &filename, const QDateTime &date=QDateTime(), const QString &baseFolder=QString(), MusicLibraryProgressMonitor *prog=0);
    quint32 fromXML(QXmlStreamReader &reader, const QDateTime &date=QDateTime(), const QString &baseFolder=QString(), MusicLibraryProgressMonitor *prog=0);
    Type itemType() const { return Type_Root; }
    void add(const QSet<Song> &songs);
    bool supportsAlbumArtistTag() { return supportsAlbumArtist; }
    void setSupportsAlbumArtistTag(bool s) { supportsAlbumArtist=s; }
    bool useAlbumImages() const { return albumImages; }
    void setUseAlbumImages(bool a) { albumImages=a; }
    bool useArtistImages() const { return artistImages; }
    void setUseArtistImages(bool a) { artistImages=a; }
    bool useLargeImages() const { return largeImages; }
    void setLargeImages(bool a) { largeImages=a; }
    void toggleGrouping();
    void clearItems();

protected:
    QString songArtist(const Song &s) const;

protected:
    bool supportsAlbumArtist; // TODO: ALBUMARTIST: Remove when libMPT supports album artist!

private:
    bool albumImages;
    bool artistImages;
    bool largeImages;
    QHash<QString, int> m_indexes;
};

#endif
