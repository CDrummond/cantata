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
#include "musiclibraryitemsong.h"
#include "song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

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
