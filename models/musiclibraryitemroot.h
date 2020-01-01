/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QModelIndex>
#include <QImage>
#include "musiclibraryitem.h"
#include "mpd-interface/song.h"
#include "support/icon.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class MusicLibraryItemArtist;
class MusicLibraryModel;

class MusicLibraryErrorMonitor
{
public:
    virtual ~MusicLibraryErrorMonitor() { }
    virtual void loadError(const QString &) = 0;
};

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
    MusicLibraryItemRoot(const QString &name=QString(), bool albumArtistSupport=true, bool flatHierarchy=false)
        : MusicLibraryItemContainer(name, nullptr)
        , supportsAlbumArtist(albumArtistSupport)
        , isFlat(flatHierarchy)
        , m_model(nullptr) {
    }
    ~MusicLibraryItemRoot() override { }

    virtual QIcon icon() const { return QIcon(); }
    virtual QImage image() const { return QImage(); }
    virtual Song fixPath(const Song &orig, bool) const { return orig; }
    virtual const QString & id() const { return m_itemData; }
    virtual bool canPlaySongs() const { return true; }
    virtual bool isOnlineService() const { return false; }
    virtual bool isDevice() const { return false; }
    MusicLibraryItemArtist * artist(const Song &s, bool create=true);
    MusicLibraryItemArtist * createArtist(const Song &s, bool forceComposer=false);
    void refreshIndexes();
    void remove(MusicLibraryItemArtist *artist);
    QSet<Song> allSongs(bool revertVa=false) const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    void updateSongFile(const Song &from, const Song &to);
    void toXML(const QString &filename, MusicLibraryProgressMonitor *prog=nullptr) const;
    void toXML(QXmlStreamWriter &writer, MusicLibraryProgressMonitor *prog=nullptr) const;
    bool fromXML(const QString &filename, const QString &baseFolder=QString(), MusicLibraryProgressMonitor *prog=nullptr, MusicLibraryErrorMonitor *em=nullptr);
    bool fromXML(QXmlStreamReader &reader, const QString &baseFolder=QString(), MusicLibraryProgressMonitor *prog=nullptr, MusicLibraryErrorMonitor *em=nullptr);
    Type itemType() const override { return Type_Root; }
    void add(const QSet<Song> &songs);
    bool supportsAlbumArtistTag() const { return supportsAlbumArtist; }
    void setSupportsAlbumArtistTag(bool s) { supportsAlbumArtist=s; }
    void clearItems();
    void setModel(MusicLibraryModel *m) { m_model=m; }
    bool flat() const { return isFlat; }

    virtual QModelIndex index() const { return QModelIndex(); }
    bool update(const QSet<Song> &songs);
    const MusicLibraryItem * findSong(const Song &s) const;
    bool songExists(const Song &s) const;
    bool updateSong(const Song &orig, const Song &edit);
    void addSongToList(const Song &s);
    void removeSongFromList(const Song &s);
    static QString artistName(const Song &s, bool forceComposer=false);

protected:
    QString songArtist(const Song &s, bool isLoadingCache=false) const;
    MusicLibraryItemArtist * getArtist(const QString &key) const;

protected:
    bool supportsAlbumArtist; // TODO: ALBUMARTIST: Remove when libMPT supports album artist!
    bool isFlat;
    mutable QHash<QString, int> m_indexes;
    MusicLibraryModel *m_model;
};

#endif
