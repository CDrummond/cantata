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

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "song.h"
#include "localize.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>

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
    MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, this);
    m_indexes.insert(aa, m_childItems.count());
    m_childItems.append(item);
    return item;
}

void MusicLibraryItemRoot::groupSingleTracks()
{
    if (!supportsAlbumArtist) {
        return;
    }

    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;
    bool created=false;

    for (; it!=m_childItems.end(); ) {
        if (various!=(*it) && static_cast<MusicLibraryItemArtist *>(*it)->allSingleTrack()) {
            if (!various) {
                QString artist=i18n("Various Artists");
                QHash<QString, int>::ConstIterator it=m_indexes.find(artist);
                if (m_indexes.end()==it) {
                    various=new MusicLibraryItemArtist(artist, this);
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

void MusicLibraryItemRoot::groupMultipleArtists()
{
    if (!supportsAlbumArtist) {
        return;
    }

    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;
    bool created=false;
    QString va=i18n("Various Artists");
    bool checkDiffVaString=va!=QLatin1String("Various Artists");

    // When grouping multiple artists - if 'Various Artists' is spelt different in curernt language, then we also need to place
    // items by 'Various Artists' into i18n('Various Artists')
    for (; it!=m_childItems.end(); ) {
        if (various!=(*it) && (!static_cast<MusicLibraryItemArtist *>(*it)->isVarious() ||
                               (checkDiffVaString && static_cast<MusicLibraryItemArtist *>(*it)->isVarious() && va!=(*it)->data())) ) {
            QList<MusicLibraryItem *> mutipleAlbums=static_cast<MusicLibraryItemArtist *>(*it)->mutipleArtistAlbums();
            if (mutipleAlbums.count()) {
                if (!various) {
                    QHash<QString, int>::ConstIterator it=m_indexes.find(va);
                    if (m_indexes.end()==it) {
                        various=new MusicLibraryItemArtist(va, this);
                        created=true;
                    } else {
                        various=static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it));
                    }
                }

                foreach (MusicLibraryItem *i, mutipleAlbums) {
                    i->setParent(various);
                    static_cast<MusicLibraryItemAlbum *>(i)->setIsMultipleArtists();
                }

                if (0==(*it)->childCount()) {
                    delete (*it);
                    it=m_childItems.erase(it);
                    continue;
                } else {
                    static_cast<MusicLibraryItemArtist *>(*it)->updateIndexes();
                }
            }
        }
        ++it;
    }

    if (various) {
        various->updateIndexes();
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
    if (supportsAlbumArtist && !s.file.isEmpty()) {
        QHash<QString, int>::ConstIterator it=m_indexes.find(i18n("Various Artists"));

        if (m_indexes.end()!=it) {
            return static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it))->isFromSingleTracks(s);
        }
    }
    return false;
}

void MusicLibraryItemRoot::refreshIndexes()
{
    m_indexes.clear();
    int i=0;
    foreach (MusicLibraryItem *item, m_childItems) {
        m_indexes.insert(item->data(), i++);
    }
}

void MusicLibraryItemRoot::remove(MusicLibraryItemArtist *artist)
{
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
}

QSet<Song> MusicLibraryItemRoot::allSongs(bool revertVa) const
{
    QSet<Song> songs;

    foreach (const MusicLibraryItem *artist, m_childItems) {
        foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(artist)->childItems()) {
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
    return songs;
}

void MusicLibraryItemRoot::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (const MusicLibraryItem *artist, m_childItems) {
        foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(artist)->childItems()) {
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                const Song &s=static_cast<const MusicLibraryItemSong *>(song)->song();
                artists.insert(s.artist);
                albumArtists.insert(s.albumArtist());
                albums.insert(s.album);
                if (!s.genre.isEmpty()) {
                    genres.insert(s.genre);
                }
            }
        }
    }
}

void MusicLibraryItemRoot::updateSongFile(const Song &from, const Song &to)
{
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

static quint32 constVersion=8;
static QLatin1String constTopTag("CantataLibrary");

void MusicLibraryItemRoot::toXML(const QString &filename, const QDateTime &date, MusicLibraryProgressMonitor *prog) const
{
    // If saving device cache, and we have NO items, then remove cache file...
    if (0==childCount() && date==QDateTime()) {
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
    toXML(writer, date, prog);
    compressor.close();
}

void MusicLibraryItemRoot::toXML(QXmlStreamWriter &writer, const QDateTime &date, MusicLibraryProgressMonitor *prog) const
{
    quint64 total=0;
    quint64 count=0;
    writer.writeStartDocument();
    QString unknown=i18n("Unknown");

    //Start with the document
    writer.writeStartElement(constTopTag);
    writer.writeAttribute("version", QString::number(constVersion));
    writer.writeAttribute("date", QString::number(date.toTime_t()));
    writer.writeAttribute("groupSingle", MPDParseUtils::groupSingle() ? "true" : "false");
    writer.writeAttribute("groupMultiple", MPDParseUtils::groupMultiple() ? "true" : "false");

    foreach (const MusicLibraryItem *a, childItems()) {
        foreach (const MusicLibraryItem *al, static_cast<const MusicLibraryItemArtist *>(a)->childItems()) {
            total+=al->childCount();
            if (prog && prog->wasStopped()) {
                return;
            }
        }
    }

    writer.writeAttribute("numTracks", QString::number(total));
    if (prog) {
        prog->writeProgress(0.0);
    }
    //Loop over all artist, albums and tracks.
    foreach (const MusicLibraryItem *a, childItems()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        writer.writeStartElement("Artist");
        writer.writeAttribute("name", artist->data());
        if (!artist->imageUrl().isEmpty()) {
            writer.writeAttribute("img", artist->imageUrl());
        }
        foreach (const MusicLibraryItem *al, artist->childItems()) {
            if (prog && prog->wasStopped()) {
                return;
            }
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            QString albumGenre=!album->childItems().isEmpty() ? static_cast<const MusicLibraryItemSong *>(album->childItems().at(0))->song().genre : QString();
            writer.writeStartElement("Album");
            writer.writeAttribute("name", album->data());
            writer.writeAttribute("year", QString::number(album->year()));
            if (!albumGenre.isEmpty() && albumGenre!=unknown) {
                writer.writeAttribute("genre", albumGenre);
            }
            if (album->isSingleTracks()) {
                writer.writeAttribute("singleTracks", "true");
            } else if (album->isMultipleArtists()) {
                writer.writeAttribute("multipleArtists", "true");
            }
            if (!album->imageUrl().isEmpty()) {
                writer.writeAttribute("img", album->imageUrl());
            }
            foreach (const MusicLibraryItem *t, album->childItems()) {
                const MusicLibraryItemSong *track = static_cast<const MusicLibraryItemSong *>(t);
                bool wroteArtist=false;
                writer.writeEmptyElement("Track");
                if (!track->song().title.isEmpty()) {
                    writer.writeAttribute("name", track->song().title);
                }
                writer.writeAttribute("file", track->file());
                if (0!=track->time()) {
                    writer.writeAttribute("time", QString::number(track->time()));
                }
                //Only write track number if it is set
                if (track->track() != 0) {
                    writer.writeAttribute("track", QString::number(track->track()));
                }
                if (track->disc() != 0) {
                    writer.writeAttribute("disc", QString::number(track->disc()));
                }
                if (!track->song().artist.isEmpty() && track->song().artist!=artist->data()) {
                    writer.writeAttribute("artist", track->song().artist);
                    wroteArtist=true;
                }
                if (!track->song().albumartist.isEmpty() && track->song().albumartist!=artist->data()) {
                    writer.writeAttribute("albumartist", track->song().albumartist);
                }
//                 writer.writeAttribute("id", QString::number(track->song().id));
                if (!track->song().genre.isEmpty() && track->song().genre!=albumGenre && track->song().genre!=unknown) {
                    writer.writeAttribute("genre", track->song().genre);
                }
                if (album->isSingleTracks()) {
                    writer.writeAttribute("album", track->song().album);
                } else if (!wroteArtist && album->isMultipleArtists() && !track->song().artist.isEmpty() && track->song().artist!=artist->data()) {
                    writer.writeAttribute("artist", track->song().artist);
                }
                if (Song::Playlist==track->song().type) {
                    writer.writeAttribute("playlist", "true");
                }
                if (track->song().year != album->year()) {
                    writer.writeAttribute("year", QString::number(track->song().year));
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

quint32 MusicLibraryItemRoot::fromXML(const QString &filename, const QDateTime &date, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
{
    QFile file(filename);
    QtIOCompressor compressor(&file);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (!compressor.open(QIODevice::ReadOnly)) {
        return 0;
    }

    QXmlStreamReader reader(&compressor);
    quint32 rv=fromXML(reader, date, baseFolder, prog);
    compressor.close();
    return rv;
}

quint32 MusicLibraryItemRoot::fromXML(QXmlStreamReader &reader, const QDateTime &date, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
{
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;
    Song song;
    quint32 xmlDate=0;
    QString unknown=i18n("Unknown");
    quint64 total=0;
    quint64 count=0;
    bool gs=MPDParseUtils::groupSingle();
    bool gm=MPDParseUtils::groupMultiple();

    if (prog) {
        prog->readProgress(0.0);
    }

    while (!reader.atEnd() && (!prog || !prog->wasStopped())) {
        reader.readNext();

        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value("version").toString().toUInt();
                xmlDate = attributes.value("date").toString().toUInt();
                gs = QLatin1String("true")==attributes.value("groupSingle").toString();
                gm = QLatin1String("true")==attributes.value("groupMultiple").toString();

                if ( version < constVersion || (date.isValid() && xmlDate < date.toTime_t())) {
                    return 0;
                }
                if (prog) {
                    total=attributes.value("numTracks").toString().toUInt();
                }
            } else if (QLatin1String("Artist")==element) {
                song.type=Song::Standard;
                song.artist=song.albumartist=attributes.value("name").toString();
                artistItem = createArtist(song);
                QString img = attributes.value("img").toString();
                if (!img.isEmpty()) {
                    artistItem->setImageUrl(img);
                }
            } else if (QLatin1String("Album")==element) {
                song.album=attributes.value("name").toString();
                song.year=attributes.value("year").toString().toUInt();
                song.genre=attributes.value("genre").toString();
                if (!song.file.isEmpty()) {
                    song.file.append("dummy.mp3");
                }
                albumItem = artistItem->createAlbum(song);
                QString img = attributes.value("img").toString();
                if (!img.isEmpty()) {
                    albumItem->setImageUrl(img);
                }
                if (QLatin1String("true")==attributes.value("singleTracks").toString()) {
                    albumItem->setIsSingleTracks();
                    song.type=Song::SingleTracks;
                } else if (QLatin1String("true")==attributes.value("multipleArtists").toString()) {
                    albumItem->setIsMultipleArtists();
                    song.type=Song::MultipleArtists;
                } else {
                    song.type=Song::Standard;
                }
            } else if (QLatin1String("Track")==element) {
                song.title=attributes.value("name").toString();
                song.file=attributes.value("file").toString();
                if (QLatin1String("true")==attributes.value("playlist").toString()) {
                    song.type=Song::Playlist;
                    songItem = new MusicLibraryItemSong(song, albumItem);
                    albumItem->append(songItem);
                    song.type=Song::Standard;
                } else {
                    if (!baseFolder.isEmpty() && song.file.startsWith(baseFolder)) {
                        song.file=song.file.mid(baseFolder.length());
                    }
                    if (attributes.hasAttribute("genre")) {
                        song.genre=attributes.value("genre").toString();
                    } else if (song.genre.isEmpty()) {
                        song.genre=unknown;
                    }
                    if (attributes.hasAttribute("artist")) {
                        song.artist=attributes.value("artist").toString();
                    } else {
                        song.artist=artistItem->data();
                    }
                    if (attributes.hasAttribute("albumartist")) {
                        song.albumartist=attributes.value("albumartist").toString();
                    } else {
                        song.albumartist=artistItem->data();
                    }

                    // Fix cache error - where MusicLibraryItemSong::data() was saved as name instead of song.name!!!!
                    if (!song.albumartist.isEmpty() && !song.artist.isEmpty() && song.albumartist!=song.artist &&
                        song.title.startsWith(song.artist+QLatin1String(" - "))) {
                        song.title=song.title.mid(song.artist.length()+3);
                    }

                    QString str=attributes.value("track").toString();
                    song.track=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value("disc").toString();
                    song.disc=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value("time").toString();
                    song.time=str.isEmpty() ? 0 : str.toUInt();
                    str=attributes.value("year").toString();
                    if (!str.isEmpty()) {
                        song.year=str.toUInt();
                    }
    //                 str=attributes.value("id").toString();
    //                 song.id=str.isEmpty() ? 0 : str.toUInt();

                    if (albumItem->isSingleTracks()) {
                        str=attributes.value("album").toString();
                        if (!str.isEmpty()) {
                            song.album=str;
                        }
                    } else if (albumItem->isMultipleArtists()) {
                        str=attributes.value("artist").toString();
                        if (!str.isEmpty()) {
                            song.artist=str;
                        }
                    }

                    song.fillEmptyFields();
                    songItem = new MusicLibraryItemSong(song, albumItem);
                    albumItem->append(songItem);
                    albumItem->addGenre(song.genre);
                    artistItem->addGenre(song.genre);
                    addGenre(song.genre);

                    if (prog && !prog->wasStopped() && total>0) {
                        count++;
                        prog->readProgress((count*100.0)/(total*1.0));
                    }
                }
            }
        }
    }

    // Grouping has changed!
    if (gs!=MPDParseUtils::groupSingle() || gm!=MPDParseUtils::groupMultiple()) {
        if (gs && gm) {
            // Now using both groupings, where previously we were using none.
            // ...so, jsut apply grouping!
            groupSingleTracks();
            groupMultipleArtists();
        } else {
            // Mixed groping, so need to redo from scratch...
            toggleGrouping();
        }
    }

    return xmlDate;
}

void MusicLibraryItemRoot::add(const QSet<Song> &songs)
{
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

        MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
        albumItem->append(songItem);
        albumItem->addGenre(s.genre);
        artistItem->addGenre(s.genre);
        addGenre(s.genre);
    }
}

void MusicLibraryItemRoot::toggleGrouping()
{
    // Grouping has changed, so we need to recreate whole structure from list of songs.
    QSet<Song> songs=allSongs();
    clearItems();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;

    foreach (Song currentSong, songs) {
        if (Song::Standard!=currentSong.type && Song::Playlist!=currentSong.type) {
            currentSong.type=MPDConnection::isPlaylist(currentSong.file) ? Song::Playlist : Song::Standard;
        }

        if (!artistItem || currentSong.albumArtist()!=artistItem->data()) {
            artistItem = artist(currentSong);
        }
        if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.album!=albumItem->data()) {
            albumItem = artistItem->album(currentSong);
        }

        songItem = new MusicLibraryItemSong(currentSong, albumItem);
        albumItem->append(songItem);
        albumItem->addGenre(currentSong.genre);
        artistItem->addGenre(currentSong.genre);
        addGenre(currentSong.genre);
    }

    // Library rebuilt, now apply any grouping...
    if (MPDParseUtils::groupSingle()) {
        groupSingleTracks();
    }
    if (MPDParseUtils::groupMultiple()) {
        groupMultipleArtists();
    }
}

void MusicLibraryItemRoot::clearItems()
{
    m_childItems.clear();
    m_indexes.clear();
    m_genres.clear();
    qDeleteAll(m_childItems);
}

QString MusicLibraryItemRoot::songArtist(const Song &s)
{
    if (!supportsAlbumArtist) {
        return s.artist;
    }

    if (Song::Standard==s.type || (Song::Playlist==s.type && !s.albumArtist().isEmpty())) {
        return s.albumArtist();
    }
    return i18n("Various Artists");
}
