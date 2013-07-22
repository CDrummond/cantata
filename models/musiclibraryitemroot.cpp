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
    MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, this);
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
    if (!supportsAlbumArtist || isFlat) {
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
    if (!isFlat && (supportsAlbumArtist && !s.file.isEmpty())) {
        QHash<QString, int>::ConstIterator it=m_indexes.find(i18n("Various Artists"));

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

void MusicLibraryItemRoot::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    foreach (const MusicLibraryItem *child, m_childItems) {
        if (MusicLibraryItem::Type_Song==child->itemType()) {
            const Song &s=static_cast<const MusicLibraryItemSong *>(child)->song();
            artists.insert(s.artist);
            albumArtists.insert(s.albumArtist());
            albums.insert(s.album);
            if (!s.genre.isEmpty()) {
                genres.insert(s.genre);
            }
        } else {
            foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(child)->childItems()) {
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

static quint32 constVersion=8;
static QLatin1String constTopTag("CantataLibrary");

void MusicLibraryItemRoot::toXML(const QString &filename, const QDateTime &date, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

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

static const QString constArtistElement=QLatin1String("Artist");
static const QString constAlbumElement=QLatin1String("Album");
static const QString constTrackElement=QLatin1String("Track");
static const QString constNameAttribute=QLatin1String("name");
static const QString constArtistAttribute=QLatin1String("artist");
static const QString constAlbumArtistAttribute=QLatin1String("albumartist");
static const QString constAlbumAttribute=QLatin1String("album");
static const QString constTrackAttribute=QLatin1String("track");
static const QString constGenreAttribute=QLatin1String("genre");
static const QString constYearAttribute=QLatin1String("year");
static const QString constTimeAttribute=QLatin1String("time");
static const QString constDiscAttribute=QLatin1String("disc");
static const QString constFileAttribute=QLatin1String("file");
static const QString constPlaylistAttribute=QLatin1String("playlist");
static const QString constGuessedAttribute=QLatin1String("guessed");
static const QString constDateAttribute=QLatin1String("date");
static const QString constVersionAttribute=QLatin1String("version");
static const QString constGroupSingleAttribute=QLatin1String("groupSingle");
static const QString constGroupMultipleAttribute=QLatin1String("groupMultiple");
static const QString constSingleTracksAttribute=QLatin1String("singleTracks");
static const QString constMultipleArtistsAttribute=QLatin1String("multipleArtists");
static const QString constImageAttribute=QLatin1String("img");
static const QString constnumTracksAttribute=QLatin1String("numTracks");
static const QString constTrueValue=QLatin1String("true");

void MusicLibraryItemRoot::toXML(QXmlStreamWriter &writer, const QDateTime &date, MusicLibraryProgressMonitor *prog) const
{
    if (isFlat) {
        return;
    }

    quint64 total=0;
    quint64 count=0;
    writer.writeStartDocument();
    QString unknown=i18n("Unknown");

    //Start with the document
    writer.writeStartElement(constTopTag);
    writer.writeAttribute(constVersionAttribute, QString::number(constVersion));
    writer.writeAttribute(constDateAttribute, QString::number(date.toTime_t()));
    if (MPDParseUtils::groupSingle()) {
        writer.writeAttribute(constGroupSingleAttribute, constTrueValue);
    }
    if (MPDParseUtils::groupMultiple()) {
        writer.writeAttribute(constGroupMultipleAttribute, constTrueValue);
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
        if (!artist->imageUrl().isEmpty()) {
            writer.writeAttribute(constImageAttribute, artist->imageUrl());
        }
        foreach (const MusicLibraryItem *al, artist->childItems()) {
            if (prog && prog->wasStopped()) {
                return;
            }
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            QString albumGenre=!album->childItems().isEmpty() ? static_cast<const MusicLibraryItemSong *>(album->childItems().at(0))->song().genre : QString();
            writer.writeStartElement(constAlbumElement);
            writer.writeAttribute(constNameAttribute, album->data());
            writer.writeAttribute(constYearAttribute, QString::number(album->year()));
            if (!albumGenre.isEmpty() && albumGenre!=unknown) {
                writer.writeAttribute(constGenreAttribute, albumGenre);
            }
            if (album->isSingleTracks()) {
                writer.writeAttribute(constSingleTracksAttribute, constTrueValue);
            } else if (album->isMultipleArtists()) {
                writer.writeAttribute(constMultipleArtistsAttribute, constTrueValue);
            }
            if (!album->imageUrl().isEmpty()) {
                writer.writeAttribute(constImageAttribute, album->imageUrl());
            }
            foreach (const MusicLibraryItem *t, album->childItems()) {
                const MusicLibraryItemSong *track = static_cast<const MusicLibraryItemSong *>(t);
                bool wroteArtist=false;
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
                if (!track->song().artist.isEmpty() && track->song().artist!=artist->data()) {
                    writer.writeAttribute(constArtistAttribute, track->song().artist);
                    wroteArtist=true;
                }
                if (track->song().albumartist!=artist->data()) {
                    writer.writeAttribute(constAlbumArtistAttribute, track->song().albumartist);
                }
//                 writer.writeAttribute("id", QString::number(track->song().id));
                if (!track->song().genre.isEmpty() && track->song().genre!=albumGenre && track->song().genre!=unknown) {
                    writer.writeAttribute(constGenreAttribute, track->song().genre);
                }
                if (album->isSingleTracks()) {
                    writer.writeAttribute(constAlbumAttribute, track->song().album);
                } else if (!wroteArtist && album->isMultipleArtists() && !track->song().artist.isEmpty() && track->song().artist!=artist->data()) {
                    writer.writeAttribute(constArtistAttribute, track->song().artist);
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

quint32 MusicLibraryItemRoot::fromXML(const QString &filename, const QDateTime &date, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
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
    quint32 rv=fromXML(reader, date, baseFolder, prog);
    compressor.close();
    #ifdef TIME_XML_FILE_LOADING
    qWarning() << filename << timer.elapsed();
    #endif
    return rv;
}

quint32 MusicLibraryItemRoot::fromXML(QXmlStreamReader &reader, const QDateTime &date, const QString &baseFolder, MusicLibraryProgressMonitor *prog)
{
    if (isFlat) {
        return 0;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    Song song;
    quint32 xmlDate=0;
    QString unknown=i18n("Unknown");
    QString lastGenre;
    quint64 total=0;
    quint64 count=0;
    bool gs=MPDParseUtils::groupSingle();
    bool gm=MPDParseUtils::groupMultiple();
    int percent=0;

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
                gm = constTrueValue==attributes.value(constGroupMultipleAttribute).toString();

                if ( version < constVersion || (date.isValid() && xmlDate < date.toTime_t())) {
                    return 0;
                }
                if (prog) {
                    total=attributes.value(constnumTracksAttribute).toString().toUInt();
                }
            } else if (constArtistElement==element) {
                song.type=Song::Standard;
                song.artist=song.albumartist=attributes.value(constNameAttribute).toString();
                artistItem = createArtist(song);
                QString img = attributes.value(constImageAttribute).toString();
                if (!img.isEmpty()) {
                    artistItem->setImageUrl(img);
                }
            } else if (constAlbumElement==element) {
                song.album=attributes.value(constNameAttribute).toString();
                song.year=attributes.value(constYearAttribute).toString().toUInt();
                song.genre=attributes.value(constGenreAttribute).toString();
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
                } else if (constTrueValue==attributes.value(constMultipleArtistsAttribute).toString()) {
                    albumItem->setIsMultipleArtists();
                    song.type=Song::MultipleArtists;
                } else {
                    song.type=Song::Standard;
                }
            } else if (constTrackElement==element) {
                song.title=attributes.value(constNameAttribute).toString();
                song.file=attributes.value(constFileAttribute).toString();
                if (constTrueValue==attributes.value(constPlaylistAttribute).toString()) {
                    song.type=Song::Playlist;
                    albumItem->append(new MusicLibraryItemSong(song, albumItem));
                    song.type=Song::Standard;
                } else {
                    if (!baseFolder.isEmpty() && song.file.startsWith(baseFolder)) {
                        song.file=song.file.mid(baseFolder.length());
                    }
                    song.genre=attributes.value(constGenreAttribute).toString();
                    if (song.genre.isEmpty()) {
                        song.genre=unknown;
                    }
                    song.artist=attributes.value(constArtistAttribute).toString();
                    if (song.artist.isEmpty()) {
                        song.artist=artistItem->data();
                    }
                    song.albumartist=attributes.value(constAlbumArtistAttribute).toString();
                    if (song.albumartist.isEmpty()) {
                        song.albumartist=artistItem->data();
                    }

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
                    } else if (albumItem->isMultipleArtists()) {
                        str=attributes.value(constArtistAttribute).toString();
                        if (!str.isEmpty()) {
                            song.artist=str;
                        }
                    }

                    song.fillEmptyFields();
                    if (constTrueValue==attributes.value(constGuessedAttribute).toString()) {
                        song.guessed=true;
                    }
                    albumItem->append(new MusicLibraryItemSong(song, albumItem));
                    if (song.genre!=lastGenre) {
                        albumItem->addGenre(song.genre);
                        artistItem->addGenre(song.genre);
                        addGenre(song.genre);
                        lastGenre=song.genre;
                    }

                    if (prog && !prog->wasStopped() && total>0) {
                        count++;
                        int pc=((count*100.0)/(total*1.0))+0.5;
                        if (pc!=percent) {
                            prog->readProgress(pc);
                            percent=pc;
                        }
                    }
                }
            }
        }
    }

    // Grouping has changed!
    if (gs!=MPDParseUtils::groupSingle() || gm!=MPDParseUtils::groupMultiple()) {
        if (gs && gm) {
            // Now using both groupings, where previously we were using none.
            // ...so, just apply grouping!
            groupSingleTracks();
            groupMultipleArtists();
        } else {
            // Mixed grouping, so need to redo from scratch...
            toggleGrouping();
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
        albumItem->addGenre(s.genre);
        artistItem->addGenre(s.genre);
        addGenre(s.genre);
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

        if (!artistItem || currentSong.albumArtist()!=artistItem->data()) {
            artistItem = artist(currentSong);
        }
        if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.album!=albumItem->data()) {
            albumItem = artistItem->album(currentSong);
        }

        albumItem->append(new MusicLibraryItemSong(currentSong, albumItem));
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
        MusicLibraryItemArtist *artistItem = ((MusicLibraryItemRoot *)this)->artist(s, false);
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
        mod.albumartist=i18n("Various Artists");
        if (MPDParseUtils::groupMultiple()) {
            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist()) {
                    return true;
                }
            }
        }
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
            if (static_cast<MusicLibraryItemSong *>(song)->song()==orig) {
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
            if (static_cast<MusicLibraryItemSong *>(song)->song()==orig) {
                static_cast<MusicLibraryItemSong *>(song)->setSong(edit);
                if (orig.genre!=edit.genre) {
                    albumItem->updateGenres();
                    artistItem->updateGenres();
                    updateGenres();
                }
                QModelIndex idx=m_model->createIndex(songRow, 0, song);
                emit m_model->dataChanged(idx, idx);
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
        m_model->endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        m_model->beginInsertRows(m_model->createIndex(childItems().indexOf(artistItem), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        m_model->endInsertRows();
    }
    quint32 year=albumItem->year();
    foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
        const MusicLibraryItemSong *song=static_cast<const MusicLibraryItemSong *>(songItem);
        if (song->track()==s.track && song->disc()==s.disc && song->data()==s.displayTitle()) {
            return;
        }
    }

    m_model->beginInsertRows(m_model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    m_model->endInsertRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=m_model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem);
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
        if (static_cast<MusicLibraryItemSong *>(song)->song().title==s.title) {
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
        int row=m_childItems.indexOf(artistItem);
        m_model->beginRemoveRows(index(), row, row);
        remove(artistItem);
        m_model->endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=artistItem->childItems().indexOf(albumItem);
        m_model->beginRemoveRows(m_model->createIndex(childItems().indexOf(artistItem), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        m_model->endRemoveRows();
        return;
    }

    // Just remove particular song
    m_model->beginRemoveRows(m_model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), songRow, songRow);
    quint32 year=albumItem->year();
    albumItem->remove(songRow);
    m_model->endRemoveRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=m_model->createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem);
        emit m_model->dataChanged(idx, idx);
    }
}

void MusicLibraryItemRoot::clearImages()
{
    foreach (MusicLibraryItem *i, m_childItems) {
        if (MusicLibraryItem::Type_Artist==i->itemType()) {
            static_cast<MusicLibraryItemArtist *>(i)->clearImages();
        }
    }
}

QString MusicLibraryItemRoot::songArtist(const Song &s) const
{
    if (isFlat || !supportsAlbumArtist) {
        return s.artist;
    }

    if (Song::Standard==s.type || (Song::Playlist==s.type && !s.albumArtist().isEmpty())) {
        return s.albumArtist();
    }
    return i18n("Various Artists");
}
