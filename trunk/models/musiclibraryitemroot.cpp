/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "song.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include <QtCore/QDebug>

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s)
{
    QString aa=s.albumArtist();
    QHash<QString, int>::ConstIterator it=m_indexes.find(aa);

    if (m_indexes.end()==it) {
        MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, this);
        m_indexes.insert(aa, m_childItems.count());
        m_childItems.append(item);
        return item;
    }
    return static_cast<MusicLibraryItemArtist *>(m_childItems.at(*it));
}

void MusicLibraryItemRoot::groupSingleTracks()
{
    QList<MusicLibraryItem *>::iterator it=m_childItems.begin();
    MusicLibraryItemArtist *various=0;

    for (; it!=m_childItems.end(); ) {
        if ((!various || various!=(*it)) && static_cast<MusicLibraryItemArtist *>(*it)->allSingleTrack()) {
            if (!various) {
                Song s;
                #ifdef ENABLE_KDE_SUPPORT
                s.artist=i18n("Various Artists");
                #else
                s.artist=QObject::tr("Various Artists");
                #endif
                various=artist(s);
            }
            various->addToSingleTracks(static_cast<MusicLibraryItemArtist *>(*it));
            QList<MusicLibraryItem *>::iterator cur=it;
            ++it;
            delete *cur;
            m_childItems.erase(cur);
        } else {
            ++it;
        }
    }

    if (various) {
        various->sortSingle();
        it=m_childItems.begin();
        QList<MusicLibraryItem *>::iterator  end=m_childItems.end();
        for (int i=0; it!=end; ++it, ++i) {
            m_indexes.insert((*it)->data(), i);
        }
    }
}
