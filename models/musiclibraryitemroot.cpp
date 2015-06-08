/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdconnection.h"
#include "support/localize.h"
#include "qtiocompressor/qtiocompressor.h"
#include "musicmodel.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#include <QElapsedTimer>
#ifdef TIME_XML_FILE_LOADING
#include <QDebug>
#include <QElapsedTimer>
#endif

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s, bool create)
{
    QString aa=songArtist(s);
    MusicLibraryItemArtist *artistItem=getArtist(aa);
    return artistItem ? artistItem : (create ? createArtist(s) : 0);
}

MusicLibraryItemArtist * MusicLibraryItemRoot::createArtist(const Song &s, bool forceComposer)
{
    QString aa=songArtist(s, forceComposer);
    MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, Song::Standard==s.type ? s.albumArtist() : QString(), s.artistSortString(), this);
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
    foreach (MusicLibraryItem *item, m_childItems) {
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

    foreach (const MusicLibraryItem *child, m_childItems) {
        if (MusicLibraryItem::Type_Song==child->itemType()) {
            if (revertVa) {
                Song s=static_cast<const MusicLibraryItemSong *>(child)->song();
                s.revertVariousArtists();
                songs.insert(s);
            } else {
                songs.insert(static_cast<const MusicLibraryItemSong *>(child)->song());
            }
        } else {
            foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
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
    foreach (const MusicLibraryItem *child, m_childItems) {
        if (MusicLibraryItem::Type_Song==child->itemType()) {
            const Song &s=static_cast<const MusicLibraryItemSong *>(child)->song();
            artists.insert(s.artist);
            albumArtists.insert(s.albumArtist());
            composers.insert(s.composer());
            albums.insert(s.album);
        } else if (MusicLibraryItem::Type_Artist==child->itemType()) {
            foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    const Song &s=static_cast<const MusicLibraryItemSong *>(song)->song();
                    artists.insert(s.artist);
                    albumArtists.insert(s.albumArtist());
                    composers.insert(s.composer());
                    albums.insert(s.album);
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
            foreach (MusicLibraryItem *song, alb->childItems()) {
                if (static_cast<MusicLibraryItemSong *>(song)->file()==from.file) {
                    static_cast<MusicLibraryItemSong *>(song)->setFile(to.file);
                    return;
                }
            }
        }
    }
}

static quint32 constVersion=9;
static QLatin1String constTopTag("CantataLibrary");

void MusicLibraryItemRoot::toXML(const QString &filename, time_t date, bool dateUnreliable, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    // If saving device cache, and we have NO items, then remove cache file...
    if (0==childCount() && date<2000) {
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
    toXML(writer, date, dateUnreliable, prog);
    compressor.close();
}

static const QString constArtistElement=QLatin1String("Artist");
static const QString constAlbumElement=QLatin1String("Album");
static const QString constTrackElement=QLatin1String("Track");
static const QString constNameAttribute=QLatin1String("name");
static const QString constSortAttribute=QLatin1String("sort");
static const QString constArtistAttribute=QLatin1String("artist");
static const QString constAlbumArtistAttribute=QLatin1String("albumartist");
static const QString constComposerAttribute=QLatin1String("composer");
static const QString constAlbumAttribute=QLatin1String("album");
static const QString constActualAttribute=QLatin1String("actual");
static const QString constTrackAttribute=QLatin1String("track");
static const QString constGenreAttribute=QLatin1String("genre");
static const QString constYearAttribute=QLatin1String("year");
static const QString constTimeAttribute=QLatin1String("time");
static const QString constDiscAttribute=QLatin1String("disc");
static const QString constMbIdAttribute=QLatin1String("mbid");
static const QString constFileAttribute=QLatin1String("file");
static const QString constPlaylistAttribute=QLatin1String("playlist");
static const QString constGuessedAttribute=QLatin1String("guessed");
static const QString constDateAttribute=QLatin1String("date");
static const QString constDateUnreliableAttribute=QLatin1String("dateUnreliable");
static const QString constVersionAttribute=QLatin1String("version");
static const QString constMultipleArtistsAttribute=QLatin1String("multipleArtists");
static const QString constImageAttribute=QLatin1String("img");
static const QString constnumTracksAttribute=QLatin1String("numTracks");
static const QString constTrueValue=QLatin1String("true");

void MusicLibraryItemRoot::toXML(QXmlStreamWriter &writer, time_t date, bool dateUnreliable, MusicLibraryProgressMonitor *prog) const
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
    writer.writeAttribute(constDateAttribute, QString::number(date));
    if (dateUnreliable) {
        writer.writeAttribute(constDateUnreliableAttribute, constTrueValue);
    }
    foreach (const MusicLibraryItem *a, childItems()) {
        foreach (const MusicLibraryItem *al, static_cast<const MusicLibraryItemArtist *>(a)->childItems()) {
            total+=al->childCount();
            if (prog && prog->wasStopped()) {
                return;
            }
        }
    }

    writer.writeAttribute(constnumTracksAttribute, QString::number(total));

    //Loop over all artist, albums and tracks.
    foreach (const MusicLibraryItem *a, childItems()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        writer.writeStartElement(constArtistElement);
        writer.writeAttribute(constNameAttribute, artist->data());
        if (!artist->actualArtist().isEmpty()) {
            writer.writeAttribute(constActualAttribute, artist->actualArtist());
        }
        if (artist->hasSort()) {
            writer.writeAttribute(constSortAttribute, artist->sortString());
        }
        foreach (const MusicLibraryItem *al, artist->childItems()) {
            if (prog && prog->wasStopped()) {
                return;
            }
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            writer.writeStartElement(constAlbumElement);
            writer.writeAttribute(constNameAttribute, album->originalName().isEmpty() ? album->data() : album->originalName());
            writer.writeAttribute(constYearAttribute, QString::number(album->year()));
            if (!album->imageUrl().isEmpty()) {
                writer.writeAttribute(constImageAttribute, album->imageUrl());
            }
            if (!album->id().isEmpty()) {
                writer.writeAttribute(constMbIdAttribute, album->id());
            }
            if (album->hasSort()) {
                writer.writeAttribute(constSortAttribute, album->sortString());
            }
            QString artistName=artist->actualArtist().isEmpty() ? artist->data() : artist->actualArtist();

            foreach (const MusicLibraryItem *t, album->childItems()) {
                const MusicLibraryItemSong *track = static_cast<const MusicLibraryItemSong *>(t);
                writer.writeEmptyElement(constTrackElement);
                if (!track->song().title.isEmpty()) {
                    writer.writeAttribute(constNameAttribute, track->song().title);
                }
                writer.writeAttribute(constFileAttribute, track->file());
                if (0!=track->time()) {
                    writer.writeAttribute(constTimeAttribute, QString::number(track->time()));
                }
                //Only write track number if it is set
                if (track->track() != 0) {
                    writer.writeAttribute(constTrackAttribute, QString::number(track->track()));
                }
                if (track->disc() != 0) {
                    writer.writeAttribute(constDiscAttribute, QString::number(track->disc()));
                }
                if (!track->song().artist.isEmpty() && track->song().artist!=artistName) {
                    writer.writeAttribute(constArtistAttribute, track->song().artist);
                }
                if (supportsAlbumArtist && track->song().albumartist!=artistName) {
                    writer.writeAttribute(constAlbumArtistAttribute, track->song().albumartist);
                }
                if (!track->song().composer().isEmpty()) {
                    writer.writeAttribute(constComposerAttribute, track->song().composer());
                }
                QString trackGenre=track->multipleGenres() ? Song::combineGenres(track->allGenres()) : track->genre();
                if (!trackGenre.isEmpty() && trackGenre!=Song::unknown()) {
                    writer.writeAttribute(constGenreAttribute, track->song().genre);
                }
                if (album->isSingleTracks()) {
                    writer.writeAttribute(constAlbumAttribute, track->song().album);
                }
                if (Song::Playlist==track->song().type) {
                    writer.writeAttribute(constPlaylistAttribute, constTrueValue);
                }
                if (track->song().year != album->year()) {
                    writer.writeAttribute(constYearAttribute, QString::number(track->song().year));
                }
                if (track->song().guessed) {
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
            }
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
}

time_t MusicLibraryItemRoot::fromXML(const QString &filename, time_t date, bool *dateUnreliable, const QString &baseFolder, MusicLibraryProgressMonitor *prog, MusicLibraryErrorMonitor *em)
{
    if (isFlat) {
        return 0;
    }

    #ifdef TIME_XML_FILE_LOADING
    QElapsedTimer timer;
    timer.start();
    #endif

    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return 0;
    }
    QXmlStreamReader reader(&compressor);
    time_t rv=fromXML(reader, date, dateUnreliable, baseFolder, prog, em);
    compressor.close();
    #ifdef TIME_XML_FILE_LOADING
    qWarning() << filename << timer.elapsed();
    #endif
    return rv;
}

time_t MusicLibraryItemRoot::fromXML(QXmlStreamReader &reader, time_t date, bool *dateUnreliable, const QString &baseFolder, MusicLibraryProgressMonitor *prog, MusicLibraryErrorMonitor *em)
{
    if (isFlat) {
        return 0;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    Song song;
    quint32 xmlDate=0;
    quint64 total=0;
    quint64 count=0;
    int percent=0;
    bool online=isOnlineService();
    QElapsedTimer timer;

    if (prog) {
        prog->readProgress(0.0);
        timer.start();
    }

    while (!reader.atEnd() && (!prog || !prog->wasStopped())) {
        reader.readNext();

        if (reader.error()) {
            if (em) {
                em->loadError(i18n("Parse error loading cache file, please check your songs tags."));
            }
        } else if (reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value(constVersionAttribute).toString().toUInt();
                xmlDate = attributes.value(constDateAttribute).toString().toUInt();
                if ( version < constVersion || (date>0 && xmlDate < date)) {
                    return 0;
                }
                if (prog) {
                    total=attributes.value(constnumTracksAttribute).toString().toUInt();
                }
                if (dateUnreliable) {
                    *dateUnreliable=constTrueValue==attributes.value(constDateUnreliableAttribute).toString();
                }
            } else if (constArtistElement==element) {
                QString actual=attributes.value(constActualAttribute).toString();
                song.type=Song::Standard;
                if (actual.isEmpty()) {
                    song.artist=song.albumartist=attributes.value(constNameAttribute).toString();
                } else {
                    song.artist=song.albumartist=actual;
                    song.setComposer(attributes.value(constNameAttribute).toString());
                }
                song.setAlbumArtistSort(attributes.value(constSortAttribute).toString());
                artistItem = createArtist(song, song.hasComposer());
            } else if (constAlbumElement==element) {
                song.album=attributes.value(constNameAttribute).toString();
                song.year=attributes.value(constYearAttribute).toString().toUInt();
                song.genre=attributes.value(constGenreAttribute).toString();
                song.setMbAlbumId(attributes.value(constMbIdAttribute).toString());
                song.setAlbumSort(attributes.value(constSortAttribute).toString());
                if (!song.file.isEmpty()) {
                    song.file.append("dummy.mp3");
                }
                albumItem = artistItem->createAlbum(song);
                QString img = attributes.value(constImageAttribute).toString();
                if (!img.isEmpty()) {
                    albumItem->setImageUrl(img);
                }
                song.type=Song::Standard;
            } else if (constTrackElement==element) {
                song.title=attributes.value(constNameAttribute).toString();
                song.file=attributes.value(constFileAttribute).toString();
                if (constTrueValue==attributes.value(constPlaylistAttribute).toString()) {
                    song.type=Song::Playlist;
                    if (0==song.time) {
                        song.time=albumItem->totalTime();
                    }
                    albumItem->append(new MusicLibraryItemSong(song, albumItem));
                    song.type=Song::Standard;
                } else {
                    if (!baseFolder.isEmpty() && song.file.startsWith(baseFolder)) {
                        song.file=song.file.mid(baseFolder.length());
                    }
                    QString genre=attributes.value(constGenreAttribute).toString();
                    if (genre.isEmpty() ) {
                        if (song.genre.isEmpty()) {
                            song.genre=Song::unknown();
                        }
                    } else {
                        song.genre=genre;
                    }
                    if (attributes.hasAttribute(constArtistAttribute)) {
                        song.artist=attributes.value(constArtistAttribute).toString();
                    } else if (artistItem->actualArtist().isEmpty()) {
                        song.artist=artistItem->data();
                    } else {
                        song.artist=artistItem->actualArtist();
                    }
                    if (supportsAlbumArtist) {
                        if (attributes.hasAttribute(constAlbumArtistAttribute)) {
                            song.albumartist=attributes.value(constAlbumArtistAttribute).toString();
                        } else if (artistItem->actualArtist().isEmpty()) {
                            song.albumartist=artistItem->data();
                        } else {
                            song.albumartist=artistItem->actualArtist();
                        }
                    }
                    QString composer=attributes.value(constComposerAttribute).toString();
                    if (!composer.isEmpty()) {
                        song.setComposer(composer);
                    }

                    QString str=attributes.value(constTrackAttribute).toString();
                    song.track=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value(constDiscAttribute).toString();
                    song.disc=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value(constTimeAttribute).toString();
                    song.time=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value(constYearAttribute).toString();
                    if (!str.isEmpty()) {
                        song.year=str.toUInt();
                    }

                    if (albumItem->isSingleTracks()) {
                        str=attributes.value(constAlbumAttribute).toString();
                        if (!str.isEmpty()) {
                            song.album=str;
                        }
                    }

                    song.fillEmptyFields();
                    song.guessed=constTrueValue==attributes.value(constGuessedAttribute).toString();
                    if (online) {
                        song.type=Song::OnlineSvrTrack;
                    }

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
                song.time=song.track=0;
                song.setComposer(QString());
            }
        }
    }

    return xmlDate;
}

void MusicLibraryItemRoot::add(const QSet<Song> &songs)
{
    if (isFlat) {
        return;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;

    foreach (const Song &s, songs) {
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

    foreach (const Song &s, removed) {
        removeSongFromList(s);
    }
    foreach (const Song &s, added) {
        addSongToList(s);
    }
    return updatedSongs;
}

const MusicLibraryItem * MusicLibraryItemRoot::findSong(const Song &s) const
{
    if (isFlat) {
        foreach (const MusicLibraryItem *songItem, childItems()) {
            if (songItem->data()==s.displayTitle()) {
                return songItem;
            }
        }
    } else {
        MusicLibraryItemArtist *artistItem = const_cast<MusicLibraryItemRoot *>(this)->artist(s, false);
        if (artistItem) {
            MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
            if (albumItem) {
                foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
                    if (songItem->data()==s.displayTitle() && static_cast<const MusicLibraryItemSong *>(songItem)->song().track==s.track) {
                        return songItem;
                    }
                }
            }
        }
    }
    return 0;
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
        foreach (MusicLibraryItem *song, childItems()) {
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
        foreach (MusicLibraryItem *song, albumItem->childItems()) {
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
    foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
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
    MusicLibraryItem *songItem=0;
    int songRow=0;
    foreach (MusicLibraryItem *song, albumItem->childItems()) {
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
        return forceComposer && s.hasComposer() ? s.composer() : s.artistOrComposer();
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
            return 0;
        }

        QHash<QString, int>::ConstIterator idx=m_indexes.find(key);

        if (m_indexes.end()==idx) {
            return 0;
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
    MusicLibraryItemArtist *ar=0;
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
