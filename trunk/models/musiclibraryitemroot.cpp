/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include <QtXml/QXmlStreamReader>
#include <QtXml/QXmlStreamWriter>
#include <QtCore/QFile>

static QString songArtist(const Song &s)
{
    if (Song::Standard==s.type) {
        return s.albumArtist();
    }
    #ifdef ENABLE_KDE_SUPPORT
    return i18n("Various Artists");
    #else
    return QObject::tr("Various Artists");
    #endif
}

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
    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;
    bool created=false;

    for (; it!=m_childItems.end(); ) {
        if (various!=(*it) && static_cast<MusicLibraryItemArtist *>(*it)->allSingleTrack()) {
            if (!various) {
                #ifdef ENABLE_KDE_SUPPORT
                QString artist=i18n("Various Artists");
                #else
                QString artist=QObject::tr("Various Artists");
                #endif
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
    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;
    bool created=false;

    for (; it!=m_childItems.end(); ) {
        if (various!=(*it) && !static_cast<MusicLibraryItemArtist *>(*it)->isVarious()) {
            QList<MusicLibraryItem *> mutipleAlbums=static_cast<MusicLibraryItemArtist *>(*it)->mutipleArtistAlbums();
            if (mutipleAlbums.count()) {
                if (!various) {
                    #ifdef ENABLE_KDE_SUPPORT
                    QString artist=i18n("Various Artists");
                    #else
                    QString artist=QObject::tr("Various Artists");
                    #endif
                    QHash<QString, int>::ConstIterator it=m_indexes.find(artist);
                    if (m_indexes.end()==it) {
                        various=new MusicLibraryItemArtist(artist, this);
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
    if (!s.file.isEmpty()) {
        #ifdef ENABLE_KDE_SUPPORT
        QHash<QString, int>::ConstIterator it=m_indexes.find(i18n("Various Artists"));
        #else
        QHash<QString, int>::ConstIterator it=m_indexes.find(QObject::tr("Various Artists"));
        #endif

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

QSet<Song> MusicLibraryItemRoot::allSongs() const
{
    QSet<Song> songs;

    foreach (const MusicLibraryItem *artist, m_childItems) {
        foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(artist)->childItems()) {
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                songs.insert(static_cast<const MusicLibraryItemSong *>(song)->song());
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

void MusicLibraryItemRoot::toXML(const QString &filename, const QDateTime &date) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    //Write the header info
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();

    //Start with the document
    writer.writeStartElement(constTopTag);
    writer.writeAttribute("version", QString::number(constVersion));
    writer.writeAttribute("date", QString::number(date.toTime_t()));
    writer.writeAttribute("groupSingle", MPDParseUtils::groupSingle() ? "true" : "false");
    writer.writeAttribute("groupMultiple", MPDParseUtils::groupMultiple() ? "true" : "false");
    //Loop over all artist, albums and tracks.
    foreach (const MusicLibraryItem *a, childItems()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        writer.writeStartElement("Artist");
        writer.writeAttribute("name", artist->data());
        foreach (const MusicLibraryItem *al, artist->childItems()) {
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            writer.writeStartElement("Album");
            writer.writeAttribute("name", album->data());
            writer.writeAttribute("year", QString::number(album->year()));
            if (album->isSingleTracks()) {
                writer.writeAttribute("singleTracks", "true");
            } else if (album->isMultipleArtists()) {
                writer.writeAttribute("multipleArtists", "true");
            }
            foreach (const MusicLibraryItem *t, album->childItems()) {
                const MusicLibraryItemSong *track = static_cast<const MusicLibraryItemSong *>(t);
                writer.writeEmptyElement("Track");
                writer.writeAttribute("name", track->data());
                writer.writeAttribute("file", track->file());
                writer.writeAttribute("time", QString::number(track->time()));
                //Only write track number if it is set
                if (track->track() != 0) {
                    writer.writeAttribute("track", QString::number(track->track()));
                }
                if (track->disc() != 0) {
                    writer.writeAttribute("disc", QString::number(track->disc()));
                }
                if (track->song().artist!=artist->data()) {
                    writer.writeAttribute("artist", track->song().artist);
                }
                if (track->song().albumartist!=artist->data()) {
                    writer.writeAttribute("albumartist", track->song().albumartist);
                }
//                 writer.writeAttribute("id", QString::number(track->song().id));
                if (!track->song().genre.isEmpty()) {
                    writer.writeAttribute("genre", track->song().genre);
                }
                if (album->isSingleTracks()) {
                    writer.writeAttribute("album", track->song().album);
                } else if (album->isMultipleArtists()) {
                    writer.writeAttribute("artist", track->song().artist);
                }
            }
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
    file.close();
}

quint32 MusicLibraryItemRoot::fromXML(const QString &filename, const QDateTime &date)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;
    Song song;
    QXmlStreamReader reader(&file);
    quint32 xmlDate=0;

    while (!reader.atEnd()) {
        reader.readNext();

        /**
         * TODO: CHECK FOR ERRORS
         */
        if (!reader.error() && reader.isStartElement()) {
            QString element = reader.name().toString();
            QXmlStreamAttributes attributes=reader.attributes();

            if (constTopTag == element) {
                quint32 version = attributes.value("version").toString().toUInt();
                xmlDate = attributes.value("date").toString().toUInt();
                bool gs = QLatin1String("true")==attributes.value("groupSingle").toString();
                bool gm = QLatin1String("true")==attributes.value("groupMultiple").toString();

                if ( version < constVersion || (date.isValid() && xmlDate < date.toTime_t()) || gs!=MPDParseUtils::groupSingle() || gm!=MPDParseUtils::groupMultiple()) {
                    return 0;
                }
            }

            if (QLatin1String("Artist")==element) {
                song.type=Song::Standard;
                song.artist=song.albumartist=attributes.value("name").toString();
                artistItem = createArtist(song);
            }
            else if (QLatin1String("Album")==element) {
                song.album=attributes.value("name").toString();
                song.year=attributes.value("year").toString().toUInt();
                if (!song.file.isEmpty()) {
                    song.file.append("dummy.mp3");
                }
                albumItem = artistItem->createAlbum(song);
                if (QLatin1String("true")==attributes.value("singleTracks").toString()) {
                    albumItem->setIsSingleTracks();
                    song.type=Song::SingleTracks;
                } else if (QLatin1String("true")==attributes.value("multipleArtists").toString()) {
                    albumItem->setIsMultipleArtists();
                    song.type=Song::MultipleArtists;
                } else {
                    song.type=Song::Standard;
                }
            }
            else if (QLatin1String("Track")==element) {
                song.title=attributes.value("name").toString();
                song.file=attributes.value("file").toString();
                song.genre=attributes.value("genre").toString();
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

                QString str=attributes.value("track").toString();
                song.track=str.isEmpty() ? 0 : str.toUInt();
                str=attributes.value("disc").toString();
                song.disc=str.isEmpty() ? 0 : str.toUInt();
                str=attributes.value("time").toString();
                song.time=str.isEmpty() ? 0 : str.toUInt();
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
            }
        }
    }

    file.close();
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

        if (!artistItem || s.albumArtist()!=artistItem->data()) {
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
