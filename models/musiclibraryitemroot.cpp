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
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "song.h"
#include "localize.h"
#include "qtiocompressor/qtiocompressor.h"
#include "musicmodel.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#ifdef TIME_XML_FILE_LOADING
#include <QDebug>
#include <QElapsedTimer>
#endif

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s, bool create)
{
    QString aa=songArtist(s);
    QHash<QString, int>::ConstIterator it=m_indexes.find(aa);

    if (m_indexes.end()==it) {
        return create ? createArtist(s) : 0;
    }
    return static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it));
}

MusicLibraryItemArtist * MusicLibraryItemRoot::createArtist(const Song &s)
{
    QString aa=songArtist(s);
    MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, Song::Standard==s.type ? s.albumArtist() : QString(), this);
    m_indexes.insert(aa, m_childItems.count());
    m_childItems.append(item);
    return item;
}

void MusicLibraryItemRoot::groupSingleTracks()
{
    if (!supportsAlbumArtist || isFlat) {
        return;
    }

    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;
    bool created=false;

    for (; it!=m_childItems.end(); ) {
        if (various!=(*it) && static_cast<MusicLibraryItemArtist *>(*it)->allSingleTrack()) {
            if (!various) {
                QHash<QString, int>::ConstIterator it=m_indexes.find(Song::variousArtists());
                if (m_indexes.end()==it) {
                    various=new MusicLibraryItemArtist(Song::variousArtists(), QString(), this);
                    created=true;
                } else {
                    various=static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it));
                }
            }
            various->addToSingleTracks(static_cast<MusicLibraryItemArtist *>(*it));
            delete (*it);
            it=m_childItems.erase(it);
        } else {
            ++it;
        }
    }

    if (various) {
        m_indexes.clear();
        if (created) {
            m_childItems.append(various);
        }
        it=m_childItems.begin();
        QList<MusicLibraryItem *>::iterator end=m_childItems.end();
        for (int i=0; it!=end; ++it, ++i) {
            m_indexes.insert((*it)->data(), i);
        }
    }
}

bool MusicLibraryItemRoot::isFromSingleTracks(const Song &s) const
{
    if (!isFlat && (supportsAlbumArtist && !s.file.isEmpty())) {
        QHash<QString, int>::ConstIterator it=m_indexes.find(Song::variousArtists());

        if (m_indexes.end()!=it) {
            return static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it))->isFromSingleTracks(s);
        }
    }
    return false;
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
            composers.insert(s.composer);
            albums.insert(s.album);
            if (!s.genre.isEmpty()) {
                genres.insert(s.genre);
            }
        } else if (MusicLibraryItem::Type_Artist==child->itemType()) {
            foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                    const Song &s=static_cast<const MusicLibraryItemSong *>(song)->song();
                    artists.insert(s.artist);
                    albumArtists.insert(s.albumArtist());
                    composers.insert(s.composer);
                    albums.insert(s.album);
                    if (!s.genre.isEmpty()) {
                        genres.insert(s.genre);
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

void MusicLibraryItemRoot::toXML(const QString &filename, const QDateTime &date, bool dateUnreliable, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    // If saving device cache, and we have NO items, then remove cache file...
    if (0==childCount() && date.date().year()<2000) {
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
static const QString constGroupSingleAttribute=QLatin1String("groupSingle");
static const QString constUseComposerAttribute=QLatin1String("useComposer");
static const QString constSingleTracksAttribute=QLatin1String("singleTracks");
static const QString constMultipleArtistsAttribute=QLatin1String("multipleArtists");
static const QString constImageAttribute=QLatin1String("img");
static const QString constnumTracksAttribute=QLatin1String("numTracks");
static const QString constTrueValue=QLatin1String("true");

void MusicLibraryItemRoot::toXML(QXmlStreamWriter &writer, const QDateTime &date, bool dateUnreliable, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    quint64 total=0;
    quint64 count=0;
    writer.writeStartDocument();

    //Start with the document
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constVersionAttribute, QString::number(constVersion));
    writer.writeAttribute(constDateAttribute, QString::number(date.toTime_t()));
    if (dateUnreliable) {
        writer.writeAttribute(constDateUnreliableAttribute, constTrueValue);
    }
    if (MPDParseUtils::groupSingle()) {
        writer.writeAttribute(constGroupSingleAttribute, constTrueValue);
    }
    if (Song::useComposer()) {
        writer.writeAttribute(constUseComposerAttribute, constTrueValue);
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
    if (prog) {
        prog->writeProgress(0.0);
    }
    //Loop over all artist, albums and tracks.
    foreach (const MusicLibraryItem *a, childItems()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        writer.writeStartElement(constArtistElement);
        writer.writeAttribute(constNameAttribute, artist->data());
        if (!artist->actualArtist().isEmpty()) {
            writer.writeAttribute(constActualAttribute, artist->actualArtist());
        }
        foreach (const MusicLibraryItem *al, artist->childItems()) {
            if (prog && prog->wasStopped()) {
                return;
            }
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            QString albumGenre=!album->childItems().isEmpty() ? static_cast<const MusicLibraryItemSong *>(album->childItems().at(0))->song().genre : QString();
            writer.writeStartElement(constAlbumElement);
            writer.writeAttribute(constNameAttribute, album->originalName().isEmpty() ? album->data() : album->originalName());
            writer.writeAttribute(constYearAttribute, QString::number(album->year()));
            if (!albumGenre.isEmpty() && albumGenre!=Song::unknown()) {
                writer.writeAttribute(constGenreAttribute, albumGenre);
            }
            if (album->isSingleTracks()) {
                writer.writeAttribute(constSingleTracksAttribute, constTrueValue);
            }
            if (!album->imageUrl().isEmpty()) {
                writer.writeAttribute(constImageAttribute, album->imageUrl());
            }
            if (!album->id().isEmpty()) {
                writer.writeAttribute(constMbIdAttribute, album->id());
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
                if (!track->song().composer.isEmpty()) {
                    writer.writeAttribute(constComposerAttribute, track->song().composer);
                }
                if (!track->song().genre.isEmpty() && track->song().genre!=albumGenre && track->song().genre!=Song::unknown()) {
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
                    prog->writeProgress((count*100.0)/(total*1.0));
                }
            }
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
}

quint32 MusicLibraryItemRoot::fromXML(const QString &filename, const QDateTime &date, bool *dateUnreliable, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
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
    quint32 rv=fromXML(reader, date, dateUnreliable, baseFolder, prog);
    compressor.close();
    #ifdef TIME_XML_FILE_LOADING
    qWarning() << filename << timer.elapsed();
    #endif
    return rv;
}

quint32 MusicLibraryItemRoot::fromXML(QXmlStreamReader &reader, const QDateTime &date, bool *dateUnreliable, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
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
    bool gs=MPDParseUtils::groupSingle();
    bool gm=false;
    int percent=0;
    bool online=isOnlineService();

    if (prog) {
        prog->readProgress(0.0);
    }

    while (!reader.atEnd() && (!prog || !prog->wasStopped())) {
        reader.readNext();

        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value(constVersionAttribute).toString().toUInt();
                xmlDate = attributes.value(constDateAttribute).toString().toUInt();
                gs = constTrueValue==attributes.value(constGroupSingleAttribute).toString();
                gm = constTrueValue==attributes.value(constGroupSingleAttribute).toString();
                bool uc = constTrueValue==attributes.value(constUseComposerAttribute).toString();
                if ( version < constVersion || uc!=Song::useComposer() || (date.isValid() && xmlDate < date.toTime_t())) {
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
                    song.composer=attributes.value(constNameAttribute).toString();
                }
                artistItem = createArtist(song);
            } else if (constAlbumElement==element) {
                song.album=attributes.value(constNameAttribute).toString();
                song.year=attributes.value(constYearAttribute).toString().toUInt();
                song.genre=attributes.value(constGenreAttribute).toString();
                song.mbAlbumId=attributes.value(constMbIdAttribute).toString();
                if (!song.file.isEmpty()) {
                    song.file.append("dummy.mp3");
                }
                albumItem = artistItem->createAlbum(song);
                QString img = attributes.value(constImageAttribute).toString();
                if (!img.isEmpty()) {
                    albumItem->setImageUrl(img);
                }
                if (constTrueValue==attributes.value(constSingleTracksAttribute).toString()) {
                    albumItem->setIsSingleTracks();
                    song.type=Song::SingleTracks;
                } else {
                    song.type=Song::Standard;
                }
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
                    song.composer=attributes.value(constComposerAttribute).toString();

                    // Fix cache error - where MusicLibraryItemSong::data() was saved as name instead of song.name!!!!
                    if (!song.albumartist.isEmpty() && !song.artist.isEmpty() && song.albumartist!=song.artist &&
                        song.title.startsWith(song.artist+QLatin1String(" - "))) {
                        song.title=song.title.mid(song.artist.length()+3);
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
    //                 str=attributes.value("id").toString();
    //                 song.id=str.isEmpty() ? 0 : str.toUInt();

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

                    MusicLibraryItemSong *songItem=new MusicLibraryItemSong(song, albumItem);
                    const QSet<QString> &songGenres=songItem->allGenres();
                    albumItem->append(songItem);
                    albumItem->addGenres(songGenres);
                    artistItem->addGenres(songGenres);
                    addGenres(songGenres);

                    if (prog && !prog->wasStopped() && total>0) {
                        count++;
                        int pc=((count*100.0)/(total*1.0))+0.5;
                        if (pc!=percent) {
                            prog->readProgress(pc);
                            percent=pc;
                        }
                    }
                }
                song.time=song.track=0;
                song.composer=QString();
            }
        }
    }

    // Grouping has changed!
    // As of 1.4.0, Cantata no-longer groups multiple-artist albums under 'Various Artists' - so if this setting
    // has been used, we need to undo it!
    if (gs!=MPDParseUtils::groupSingle() || gm) {
        toggleGrouping();
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

        MusicLibraryItemSong *songItem=new MusicLibraryItemSong(s, albumItem);
        const QSet<QString> &songGenres=songItem->allGenres();
        albumItem->append(songItem);
        albumItem->addGenres(songGenres);
        artistItem->addGenres(songGenres);
        addGenres(songGenres);
    }
}

void MusicLibraryItemRoot::toggleGrouping()
{
    if (isFlat) {
        return;
    }

    // Grouping has changed, so we need to recreate whole structure from list of songs.
    QSet<Song> songs=allSongs();
    clearItems();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;

    foreach (Song currentSong, songs) {
        if (Song::Standard!=currentSong.type && Song::Playlist!=currentSong.type) {
            currentSong.type=MPDConnection::isPlaylist(currentSong.file) ? Song::Playlist : Song::Standard;
        }

        if (!artistItem || currentSong.artistOrComposer()!=artistItem->data()) {
            artistItem = artist(currentSong);
        }
        if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.albumName()!=albumItem->data()) {
            albumItem = artistItem->album(currentSong);
        }

        MusicLibraryItemSong *song=new MusicLibraryItemSong(currentSong, albumItem);
        const QSet<QString> &songGenres=song->allGenres();
        albumItem->append(song);
        albumItem->addGenres(songGenres);
        artistItem->addGenres(songGenres);
        addGenres(songGenres);
    }

    // Library rebuilt, now apply any grouping...
    applyGrouping();
}

void MusicLibraryItemRoot::applyGrouping()
{
    if (MPDParseUtils::groupSingle()) {
        groupSingleTracks();
        updateGenres();
    }
}

void MusicLibraryItemRoot::clearItems()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_indexes.clear();
    m_genres.clear();
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
    if (updatedSongs) {
        updateGenres();
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
        if (MPDParseUtils::groupSingle()) {
            mod.album=i18n("Single Tracks");
            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist() && sng.album==s.album) {
                    return true;
                }
            }
        }
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
                if (orig.genre!=edit.genre) {
                    updateGenres();
                }
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
                if (orig.genre!=edit.genre) {
                    albumItem->updateGenres();
                    artistItem->updateGenres();
                    updateGenres();
                }
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

QString MusicLibraryItemRoot::artistName(const Song &s)
{
    if (Song::Standard==s.type || Song::Cdda==s.type || Song::OnlineSvrTrack==s.type || (Song::Playlist==s.type && !s.albumArtist().isEmpty())) {
        return s.artistOrComposer();
    }
    return Song::variousArtists();
}

QString MusicLibraryItemRoot::songArtist(const Song &s) const
{
    if (isFlat || !supportsAlbumArtist) {
        return s.artist;
    }

    return artistName(s);
}
