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
#include "song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include <QtXml/QXmlStreamReader>
#include <QtXml/QXmlStreamWriter>
#include <QtCore/QDateTime>
#include <QtCore/QFile>

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s, bool create)
{
    QString aa=s.albumArtist();
    QHash<QString, int>::ConstIterator it=m_indexes.find(aa);

    if (m_indexes.end()==it) {
        return create ? createArtist(s) : 0;
    }
    return static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it));
}

MusicLibraryItemArtist * MusicLibraryItemRoot::createArtist(const Song &s)
{
    QString aa=s.albumArtist();
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
                Song s;
                #ifdef ENABLE_KDE_SUPPORT
                s.artist=i18n("Various Artists");
                #else
                s.artist=QObject::tr("Various Artists");
                #endif
                QHash<QString, int>::ConstIterator it=m_indexes.find(s.albumArtist());
                if (m_indexes.end()==it) {
                    various=new MusicLibraryItemArtist(s.albumArtist(), this);
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
        foreach (const MusicLibraryItem *album, artist->children()) {
            foreach (const MusicLibraryItem *song, album->children()) {
                songs.insert(static_cast<const MusicLibraryItemSong *>(song)->song());
            }
        }
    }
    return songs;
}

static quint32 constVersion=5;
static QLatin1String constTopTag("CantataLibrary");

void MusicLibraryItemRoot::toXML(const QString &filename, const QDateTime &date, bool groupSingle) const
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
    writer.writeAttribute("groupSingle", groupSingle ? "true" : "false");
    //Loop over all artist, albums and tracks.
    foreach (const MusicLibraryItem *a, children()) {
        const MusicLibraryItemArtist *artist = static_cast<const MusicLibraryItemArtist *>(a);
        writer.writeStartElement("Artist");
        writer.writeAttribute("name", artist->data());
        foreach (const MusicLibraryItem *al, artist->children()) {
            const MusicLibraryItemAlbum *album = static_cast<const MusicLibraryItemAlbum *>(al);
            writer.writeStartElement("Album");
            writer.writeAttribute("name", album->data());
            writer.writeAttribute("year", QString::number(album->year()));
            if (album->isSingleTracks()) {
                writer.writeAttribute("singleTracks", "true");
            }
            foreach (const MusicLibraryItem *t, album->children()) {
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
                if (track->song().albumartist!=track->song().artist) {
                    writer.writeAttribute("artist", track->song().artist);
                }
//                 writer.writeAttribute("id", QString::number(track->song().id));
                if (!track->genre().isEmpty()) {
                    writer.writeAttribute("genre", track->genre());
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

quint32 MusicLibraryItemRoot::fromXML(const QString &filename, const QDateTime &date, bool groupSingle)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
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

            if (constTopTag == element) {
                quint32 version = reader.attributes().value("version").toString().toUInt();
                xmlDate = reader.attributes().value("date").toString().toUInt();
                bool gs = QLatin1String("true")==reader.attributes().value("groupSingle").toString();

                if ( version < constVersion || (date.isValid() && xmlDate < date.toTime_t()) || gs!=groupSingle) {
                    return 0;
                }
            }

            if (QLatin1String("Artist")==element) {
                song.albumartist=reader.attributes().value("name").toString();
                artistItem = createArtist(song);
            }
            else if (QLatin1String("Album")==element) {
                song.album=reader.attributes().value("name").toString();
                song.year=reader.attributes().value("year").toString().toUInt();
                if (!song.file.isEmpty()) {
                    song.file.append("dummy.mp3");
                }
                albumItem = artistItem->createAlbum(song);
                if (QLatin1String("true")==reader.attributes().value("singleTracks").toString()) {
                    albumItem->setIsSingleTracks();
                }
            }
            else if (QLatin1String("Track")==element) {
                song.title=reader.attributes().value("name").toString();
                song.file=reader.attributes().value("file").toString();
                song.artist=reader.attributes().value("artist").toString();
                if (song.artist.isEmpty()) {
                    song.artist=song.albumartist;
                }

                QString str=reader.attributes().value("track").toString();
                song.track=str.isEmpty() ? 0 : str.toUInt();
                str=reader.attributes().value("disc").toString();
                song.disc=str.isEmpty() ? 0 : str.toUInt();
                str=reader.attributes().value("time").toString();
                song.time=str.isEmpty() ? 0 : str.toUInt();
//                 str=reader.attributes().value("id").toString();
//                 song.id=str.isEmpty() ? 0 : str.toUInt();

                songItem = new MusicLibraryItemSong(song, albumItem);

                albumItem->append(songItem);

                QString genre = reader.attributes().value("genre").toString().trimmed();
                if (genre.isEmpty()) {
                    #ifdef ENABLE_KDE_SUPPORT
                    genre=i18n("Unknown");
                    #else
                    genre=QObject::tr("Unknown");
                    #endif
                }

                albumItem->addGenre(genre);
                artistItem->addGenre(genre);
                songItem->addGenre(genre);
                addGenre(genre);
            }
        }
    }

    file.close();
    return xmlDate;
}
