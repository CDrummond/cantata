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

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "musiclibrarymodel.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdconnection.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#include <QElapsedTimer>
//#define TIME_XML_FILE_LOADING
#ifdef TIME_XML_FILE_LOADING
#include <QDebug>
#endif

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s, bool create)
{
    QString aa=songArtist(s);
    MusicLibraryItemArtist *artistItem=getArtist(aa);
    return artistItem ? artistItem : (create ? createArtist(s) : nullptr);
}

MusicLibraryItemArtist * MusicLibraryItemRoot::createArtist(const Song &s, bool forceComposer)
{
    QString aa=songArtist(s, forceComposer);
    MusicLibraryItemArtist *item=new MusicLibraryItemArtist(s, this);
    m_indexes.insert(aa, m_childItems.count());
    m_childItems.append(item);
    return item;
}

void MusicLibraryItemRoot::refreshIndexes()
{
    if (isFlat) {
        return;
    }
    m_indexes.clear();
    int i=0;
    for (MusicLibraryItem *item: m_childItems) {
        m_indexes.insert(item->data(), i++);
    }
}

void MusicLibraryItemRoot::remove(MusicLibraryItemArtist *artist)
{
    if (isFlat) {
        return;
    }

    int index=m_childItems.indexOf(artist);

    if (index<0 || index>=m_childItems.count()) {
        return;
    }

    QHash<QString, int>::Iterator it=m_indexes.begin();
    QHash<QString, int>::Iterator end=m_indexes.end();

    for (; it!=end; ++it) {
        if ((*it)>index) {
            (*it)--;
        }
    }
    m_indexes.remove(artist->data());
    delete m_childItems.takeAt(index);
    resetRows();
}

QSet<Song> MusicLibraryItemRoot::allSongs(bool revertVa) const
{
    QSet<Song> songs;

    for (const MusicLibraryItem *child: m_childItems) {
        if (MusicLibraryItem::Type_Song==child->itemType()) {
            if (revertVa) {
                Song s=static_cast<const MusicLibraryItemSong *>(child)->song();
                s.revertVariousArtists();
                songs.insert(s);
            } else {
                songs.insert(static_cast<const MusicLibraryItemSong *>(child)->song());
            }
        } else {
            for (const MusicLibraryItem *album: static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
                for (const MusicLibraryItem *song: static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    if (revertVa) {
                        Song s=static_cast<const MusicLibraryItemSong *>(song)->song();
                        s.revertVariousArtists();
                        songs.insert(s);
                    } else {
                        songs.insert(static_cast<const MusicLibraryItemSong *>(song)->song());
                    }
                }
            }
        }
    }
    return songs;
}

void MusicLibraryItemRoot::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
{
    for (const MusicLibraryItem *child: m_childItems) {
        if (MusicLibraryItem::Type_Song==child->itemType()) {
            const Song &s=static_cast<const MusicLibraryItemSong *>(child)->song();
            artists.insert(s.artist);
            albumArtists.insert(s.albumArtist());
            composers.insert(s.composer());
            albums.insert(s.album);
            for (int i=0; i<Song::constNumGenres && !s.genres[i].isEmpty(); ++i) {
                genres+=s.genres[i];
            }
        } else if (MusicLibraryItem::Type_Artist==child->itemType()) {
            for (const MusicLibraryItem *album: static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
                for (const MusicLibraryItem *song: static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    const Song &s=static_cast<const MusicLibraryItemSong *>(song)->song();
                    artists.insert(s.artist);
                    albumArtists.insert(s.albumArtist());
                    composers.insert(s.composer());
                    albums.insert(s.album);
                    for (int i=0; i<Song::constNumGenres && !s.genres[i].isEmpty(); ++i) {
                        genres+=s.genres[i];
                    }
                }
            }
        }
    }
}

void MusicLibraryItemRoot::updateSongFile(const Song &from, const Song &to)
{
    if (isFlat) {
        return;
    }

    MusicLibraryItemArtist *art=artist(from, false);
    if (art) {
        MusicLibraryItemAlbum *alb=art->album(from, false);
        if (alb) {
            for (MusicLibraryItem *song: alb->childItems()) {
                if (static_cast<MusicLibraryItemSong *>(song)->file()==from.file) {
                    static_cast<MusicLibraryItemSong *>(song)->setFile(to.file);
                    return;
                }
            }
        }
    }
}

void MusicLibraryItemRoot::toXML(const QString &filename, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    // If saving device cache, and we have NO items, then remove cache file...
    if (0==childCount()) {
        if (QFile::exists(filename)) {
            QFile::remove(filename);
        }
        return;
    }

    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::WriteOnly)) {
        return;
    }

    QXmlStreamWriter writer(&compressor);
    toXML(writer, prog);
    compressor.close();
}

static const quint32 constVersion=1;
static const QString constTopTag=QLatin1String("tagcache");
static const QString constTrackElement=QLatin1String("track");
static const QString constTitleAttribute=QLatin1String("title");
static const QString constSortAttribute=QLatin1String("sort");
static const QString constArtistAttribute=QLatin1String("artist");
static const QString constAlbumArtistAttribute=QLatin1String("albumartist");
static const QString constArtistSortAttribute=QLatin1String("artistsort");
static const QString constAlbumArtistSortAttribute=QLatin1String("albumartistsort");
static const QString constComposerAttribute=QLatin1String("composer");
static const QString constAlbumAttribute=QLatin1String("album");
static const QString constAlbumSortAttribute=QLatin1String("albumsort");
static const QString constTrackAttribute=QLatin1String("track");
static const QString constGenreAttribute=QLatin1String("genre");
static const QString constYearAttribute=QLatin1String("year");
static const QString constTimeAttribute=QLatin1String("time");
static const QString constDiscAttribute=QLatin1String("disc");
static const QString constMbAlbumIdAttribute=QLatin1String("mbalbumid");
static const QString constFileAttribute=QLatin1String("file");
static const QString constPlaylistAttribute=QLatin1String("playlist");
static const QString constGuessedAttribute=QLatin1String("guessed");
static const QString constVersionAttribute=QLatin1String("version");
static const QString constnumTracksAttribute=QLatin1String("num");
static const QString constTrueValue=QLatin1String("true");

void MusicLibraryItemRoot::toXML(QXmlStreamWriter &writer, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    quint64 total=0;
    quint64 count=0;
    int percent=0;
    QElapsedTimer timer;
    if (prog) {
        prog->writeProgress(0.0);
        timer.start();
    }
    writer.writeStartDocument();

    //Start with the document
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constVersionAttribute, QString::number(constVersion));
    for (const MusicLibraryItem *a: childItems()) {
        for (const MusicLibraryItem *al: static_cast<const MusicLibraryItemArtist *>(a)->childItems()) {
            total+=al->childCount();
            if (prog && prog->wasStopped()) {
                return;
            }
        }
    }

    writer.writeAttribute(constnumTracksAttribute, QString::number(total));

    //Loop over all artist, albums and tracks.
    for (const MusicLibraryItem *a: childItems()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        for (const MusicLibraryItem *al: artist->childItems()) {
            if (prog && prog->wasStopped()) {
                return;
            }
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            for (const MusicLibraryItem *t: album->childItems()) {
                const MusicLibraryItemSong *track = static_cast<const MusicLibraryItemSong *>(t);
                const Song &song=track->song();
                writer.writeStartElement(constTrackElement);
                writer.writeAttribute(constFileAttribute, track->file());
                if (!song.title.isEmpty()) {
                    writer.writeAttribute(constTitleAttribute, song.title);
                }
                if (0!=track->time()) {
                    writer.writeAttribute(constTimeAttribute, QString::number(track->time()));
                }
                if (track->track()) {
                    writer.writeAttribute(constTrackAttribute, QString::number(track->track()));
                }
                if (track->disc()) {
                    writer.writeAttribute(constDiscAttribute, QString::number(track->disc()));
                }
                if (!song.artist.isEmpty()) {
                    writer.writeAttribute(constArtistAttribute, song.artist);
                }
                if (!song.albumartist.isEmpty()) {
                    writer.writeAttribute(constAlbumArtistAttribute, song.albumartist);
                }
                if (!song.album.isEmpty()) {
                    writer.writeAttribute(constAlbumAttribute, song.album);
                }
                if (song.hasComposer()) {
                    writer.writeAttribute(constComposerAttribute, song.composer());
                }
                if (song.hasMbAlbumId()) {
                    writer.writeAttribute(constMbAlbumIdAttribute, song.mbAlbumId());
                }
                if (song.hasAlbumSort()) {
                    writer.writeAttribute(constAlbumSortAttribute, song.albumSort());
                }
                if (song.hasArtistSort()) {
                    writer.writeAttribute(constArtistSortAttribute, song.artistSort());
                }
                if (song.hasAlbumArtistSort()) {
                    writer.writeAttribute(constAlbumArtistSortAttribute, song.albumArtistSort());
                }
                QString trackGenre=track->song().genres[0];
                for (int i=1; i<Song::constNumGenres && !track->song().genres[i].isEmpty(); ++i) {
                    trackGenre+=","+track->song().genres[i];
                }
                if (!trackGenre.isEmpty() && trackGenre!=Song::unknown()) {
                    writer.writeAttribute(constGenreAttribute, trackGenre);
                }
                if (Song::Playlist==song.type) {
                    writer.writeAttribute(constPlaylistAttribute, constTrueValue);
                }
                if (song.year) {
                    writer.writeAttribute(constYearAttribute, QString::number(song.year));
                }
                if (song.guessed) {
                    writer.writeAttribute(constGuessedAttribute, constTrueValue);
                }
                if (prog && !prog->wasStopped() && total>0) {
                    count++;
                    int pc=((count*100.0)/(total*1.0))+0.5;
                    if (pc!=percent && timer.elapsed()>=250) {
                        prog->writeProgress(pc);
                        timer.restart();
                        percent=pc;
                    }
                }
                writer.writeEndElement();
            }
        }
    }

    writer.writeEndElement();
    writer.writeEndDocument();
}

bool MusicLibraryItemRoot::fromXML(const QString &filename, const QString &baseFolder, MusicLibraryProgressMonitor *prog, MusicLibraryErrorMonitor *em)
{
    if (isFlat) {
        return false;
    }

    #ifdef TIME_XML_FILE_LOADING
    QElapsedTimer timer;
    timer.start();
    #endif

    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return false;
    }
    QXmlStreamReader reader(&compressor);
    bool rv=fromXML(reader, baseFolder, prog, em);
    compressor.close();
    #ifdef TIME_XML_FILE_LOADING
    qWarning() << filename << timer.elapsed();
    #endif
    return rv;
}

bool MusicLibraryItemRoot::fromXML(QXmlStreamReader &reader, const QString &baseFolder, MusicLibraryProgressMonitor *prog, MusicLibraryErrorMonitor *em)
{
    if (isFlat) {
        return false;
    }

    quint64 total=0;
    quint64 count=0;
    int percent=0;
    QElapsedTimer timer;
    bool valid=false;

    if (prog) {
        prog->readProgress(0.0);
        timer.start();
    }

    while (!reader.atEnd() && (!prog || !prog->wasStopped())) {
        reader.readNext();

        if (reader.error()) {
            if (em) {
                em->loadError(QObject::tr("Parse error loading cache file, please check your songs tags."));
            }
        } else if (reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value(constVersionAttribute).toString().toUInt();
                if (version < constVersion) {
                    return false;
                }
                if (prog) {
                    total=attributes.value(constnumTracksAttribute).toString().toUInt();
                }
                valid = true;
            } else if (valid && constTrackElement == element) {
                Song song;
                song.file=attributes.value(constFileAttribute).toString();
                if (!baseFolder.isEmpty() && song.file.startsWith(baseFolder)) {
                    song.file=song.file.mid(baseFolder.length());
                }
                if (attributes.hasAttribute(constTitleAttribute)) {
                    song.title=attributes.value(constTitleAttribute).toString();
                }
                if (attributes.hasAttribute(constTimeAttribute)) {
                    song.time=attributes.value(constTimeAttribute).toString().toUInt();
                }
                if (attributes.hasAttribute(constTrackAttribute)) {
                    song.track=attributes.value(constTrackAttribute).toString().toUInt();
                }
                if (attributes.hasAttribute(constDiscAttribute)) {
                    song.disc=attributes.value(constDiscAttribute).toString().toUInt();
                }
                if (attributes.hasAttribute(constArtistAttribute)) {
                    song.artist=attributes.value(constArtistAttribute).toString();
                }
                if (attributes.hasAttribute(constAlbumArtistAttribute)) {
                    song.albumartist=attributes.value(constAlbumArtistAttribute).toString();
                }
                if (attributes.hasAttribute(constAlbumAttribute)) {
                    song.album=attributes.value(constAlbumAttribute).toString();
                }
                if (attributes.hasAttribute(constComposerAttribute)) {
                    song.setComposer(attributes.value(constComposerAttribute).toString());
                }
                if (attributes.hasAttribute(constMbAlbumIdAttribute)) {
                    song.setMbAlbumId(attributes.value(constMbAlbumIdAttribute).toString());
                }
                if (attributes.hasAttribute(constAlbumSortAttribute)) {
                    song.setAlbumSort(attributes.value(constAlbumSortAttribute).toString());
                }
                if (attributes.hasAttribute(constArtistSortAttribute)) {
                    song.setArtistSort(attributes.value(constArtistSortAttribute).toString());
                }
                if (attributes.hasAttribute(constAlbumArtistSortAttribute)) {
                    song.setAlbumArtistSort(attributes.value(constAlbumArtistSortAttribute).toString());
                }
                if (attributes.hasAttribute(constGenreAttribute)) {
                    QStringList genres=attributes.value(constGenreAttribute).toString().split(",", QString::SkipEmptyParts);
                    for (int i=0; i<Song::constNumGenres && i<genres.count(); ++i) {
                        song.addGenre(genres[i]);
                    }
                }
                if (attributes.hasAttribute(constPlaylistAttribute) && constTrueValue==attributes.value(constPlaylistAttribute).toString()) {
                    song.type=Song::Playlist;
                }
                if (attributes.hasAttribute(constYearAttribute)) {
                    song.disc=attributes.value(constYearAttribute).toString().toUInt();
                }
                if (attributes.hasAttribute(constGuessedAttribute) && constTrueValue==attributes.value(constGuessedAttribute).toString()) {
                    song.guessed=true;
                }

                song.populateSorts();
                MusicLibraryItemAlbum *albumItem=artist(song)->album(song);
                albumItem->append(new MusicLibraryItemSong(song, albumItem));
                if (prog && !prog->wasStopped() && total>0) {
                    count++;
                    int pc=((count*100.0)/(total*1.0))+0.5;
                    if (pc!=percent && timer.elapsed()>=250) {
                        prog->readProgress(pc);
                        timer.restart();
                        percent=pc;
                    }
                }
            }
        }
    }

    return valid;
}

void MusicLibraryItemRoot::add(const QSet<Song> &songs)
{
    if (isFlat) {
        return;
    }

    MusicLibraryItemArtist *artistItem = nullptr;
    MusicLibraryItemAlbum *albumItem = nullptr;

    for (const Song &s: songs) {
        if (s.isEmpty()) {
            continue;
        }

        if (!artistItem || (supportsAlbumArtist ? s.albumArtist()!=artistItem->data() : s.album!=artistItem->data())) {
            artistItem = artist(s);
        }
        if (!albumItem || albumItem->parentItem()!=artistItem || s.album!=albumItem->data()) {
            albumItem = artistItem->album(s);
        }
        albumItem->append(new MusicLibraryItemSong(s, albumItem));
    }
}

void MusicLibraryItemRoot::clearItems()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_indexes.clear();
}

bool MusicLibraryItemRoot::update(const QSet<Song> &songs)
{
    QSet<Song> currentSongs=allSongs();
    QSet<Song> updateSongs=songs;
    QSet<Song> removed=currentSongs-updateSongs;
    QSet<Song> added=updateSongs-currentSongs;

    bool updatedSongs=added.count()||removed.count();

    for (const Song &s: removed) {
        removeSongFromList(s);
    }
    for (const Song &s: added) {
        addSongToList(s);
    }
    return updatedSongs;
}

const MusicLibraryItem * MusicLibraryItemRoot::findSong(const Song &s) const
{
    if (isFlat) {
        for (const MusicLibraryItem *songItem: childItems()) {
            if (songItem->data()==s.displayTitle()) {
                return songItem;
            }
        }
    } else {
        MusicLibraryItemArtist *artistItem = const_cast<MusicLibraryItemRoot *>(this)->artist(s, false);
        if (artistItem) {
            MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
            if (albumItem) {
                for (const MusicLibraryItem *songItem: albumItem->childItems()) {
                    if (songItem->data()==s.displayTitle() && static_cast<const MusicLibraryItemSong *>(songItem)->song().track==s.track) {
                        return songItem;
                    }
                }
            }
        }
    }
    return nullptr;
}

bool MusicLibraryItemRoot::songExists(const Song &s) const
{
    const MusicLibraryItem *song=findSong(s);

    if (song) {
        return true;
    }

    if (!s.isVariousArtists()) {
        Song mod(s);
        mod.albumartist=Song::variousArtists();
    }

    return false;
}

bool MusicLibraryItemRoot::updateSong(const Song &orig, const Song &edit)
{
    if (!m_model) {
        return false;
    }

    if (isFlat) {
        int songRow=0;
        for (MusicLibraryItem *song: childItems()) {
            if (static_cast<MusicLibraryItemSong *>(song)->song().file==orig.file) {
                static_cast<MusicLibraryItemSong *>(song)->setSong(edit);
                QModelIndex idx=m_model->createIndex(songRow, 0, song);
                emit m_model->dataChanged(idx, idx);
                return true;
            }
            songRow++;
        }
    } else if ((supportsAlbumArtist ? orig.albumArtist()==edit.albumArtist() : orig.artist==edit.artist) && orig.album==edit.album) {
        MusicLibraryItemArtist *artistItem = artist(orig, false);
        if (!artistItem) {
            return false;
        }
        MusicLibraryItemAlbum *albumItem = artistItem->album(orig, false);
        if (!albumItem) {
            return false;
        }
        int songRow=0;
        for (MusicLibraryItem *song: albumItem->childItems()) {
            if (static_cast<MusicLibraryItemSong *>(song)->song().file==orig.file) {
                static_cast<MusicLibraryItemSong *>(song)->setSong(edit);
                bool yearUpdated=orig.year!=edit.year && albumItem->updateYear();
                QModelIndex idx=m_model->createIndex(songRow, 0, song);
                emit m_model->dataChanged(idx, idx);
                if (yearUpdated) {
                    idx=m_model->createIndex(albumItem->row(), 0, albumItem);
                    emit m_model->dataChanged(idx, idx);
                }
                return true;
            }
            songRow++;
        }
    }
    return false;
}

void MusicLibraryItemRoot::addSongToList(const Song &s)
{
    if (!m_model || isFlat) {
        return;
    }

    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        m_model->beginInsertRows(index(), childCount(), childCount());
        artistItem = createArtist(s);
        artistItem->setIsNew(true);
        m_model->endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        m_model->beginInsertRows(m_model->createIndex(artistItem->row(), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        albumItem->setIsNew(true);
        m_model->endInsertRows();
        if (!artistItem->isNew()) {
            artistItem->setIsNew(true);
            QModelIndex idx=m_model->createIndex(artistItem->row(), 0, artistItem);
            emit m_model->dataChanged(idx, idx);
        }
    }
    quint32 year=albumItem->year();
    for (const MusicLibraryItem *songItem: albumItem->childItems()) {
        if (static_cast<const MusicLibraryItemSong *>(songItem)->song().file==s.file) {
            return;
        }
    }

    m_model->beginInsertRows(m_model->createIndex(albumItem->row(), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    m_model->endInsertRows();

    if (!artistItem->isNew()) {
        artistItem->setIsNew(true);
        QModelIndex idx=m_model->createIndex(artistItem->row(), 0, artistItem);
        emit m_model->dataChanged(idx, idx);
    }
    if (!albumItem->isNew()) {
        albumItem->setIsNew(true);
        QModelIndex idx=m_model->createIndex(albumItem->row(), 0, albumItem);
        emit m_model->dataChanged(idx, idx);
    } else if (year!=albumItem->year()) {
        QModelIndex idx=m_model->createIndex(albumItem->row(), 0, albumItem);
        emit m_model->dataChanged(idx, idx);
    }
}

void MusicLibraryItemRoot::removeSongFromList(const Song &s)
{
    if (!m_model || isFlat) {
        return;
    }

    MusicLibraryItemArtist *artistItem = artist(s, false);
    if (!artistItem) {
        return;
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        return;
    }
    MusicLibraryItem *songItem=nullptr;
    int songRow=0;
    for (MusicLibraryItem *song: albumItem->childItems()) {
        if (static_cast<MusicLibraryItemSong *>(song)->song().file==s.file) {
            songItem=song;
            break;
        }
        songRow++;
    }
    if (!songItem) {
        return;
    }

    if (1==artistItem->childCount() && 1==albumItem->childCount()) {
        // 1 album with 1 song - so remove whole artist
        int row=artistItem->row();
        m_model->beginRemoveRows(index(), row, row);
        remove(artistItem);
        m_model->endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=albumItem->row();
        m_model->beginRemoveRows(m_model->createIndex(artistItem->row(), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        m_model->endRemoveRows();
        return;
    }

    // Just remove particular song
    m_model->beginRemoveRows(m_model->createIndex(albumItem->row(), 0, albumItem), songRow, songRow);
    quint32 year=albumItem->year();
    albumItem->remove(songRow);
    m_model->endRemoveRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=m_model->createIndex(albumItem->row(), 0, albumItem);
        emit m_model->dataChanged(idx, idx);
    }
}

QString MusicLibraryItemRoot::artistName(const Song &s, bool forceComposer)
{
    if (Song::Standard==s.type || Song::Cdda==s.type || Song::OnlineSvrTrack==s.type || (Song::Playlist==s.type && !s.albumArtist().isEmpty())) {
        return forceComposer && s.hasComposer() ? s.composer() : s.albumArtistOrComposer();
    }
    return Song::variousArtists();
}

QString MusicLibraryItemRoot::songArtist(const Song &s, bool forceComposer) const
{
    if (isFlat || !supportsAlbumArtist) {
        return s.artist;
    }

    return artistName(s, forceComposer);
}

MusicLibraryItemArtist * MusicLibraryItemRoot::getArtist(const QString &key) const
{
    if (m_indexes.count()==m_childItems.count()) {
        if (m_childItems.isEmpty()) {
            return nullptr;
        }

        QHash<QString, int>::ConstIterator idx=m_indexes.find(key);

        if (m_indexes.end()==idx) {
            return nullptr;
        }

        // Check index value is within range
        if (*idx>=0 && *idx<m_childItems.count()) {
            MusicLibraryItemArtist *a=static_cast<MusicLibraryItemArtist *>(m_childItems.at(*idx));
            // Check id actually matches!
            if (a->data()==key) {
                return a;
            }
        }
    }

    // Something wrong with m_indexes??? So, refresh them...
    MusicLibraryItemArtist *ar=nullptr;
    m_indexes.clear();
    QList<MusicLibraryItem *>::ConstIterator it=m_childItems.constBegin();
    QList<MusicLibraryItem *>::ConstIterator end=m_childItems.constEnd();
    for (int i=0; it!=end; ++it, ++i) {
        MusicLibraryItemArtist *currenArtist=static_cast<MusicLibraryItemArtist *>(*it);
        if (!ar && currenArtist->data()==key) {
            ar=currenArtist;
        }
        m_indexes.insert(currenArtist->data(), i);
    }
    return ar;
}
