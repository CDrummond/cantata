/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "proxymodel.h"

bool ProxyModel::matchesFilter(const Song &s) const
{
    QStringList strings;

    strings << s.albumArtist();
    if (s.albumartist!=s.artist) {
        strings << s.artist;
    }
    strings << s.title << s.album;
    return matchesFilter(strings);
}

bool ProxyModel::matchesFilter(const QStringList &strings) const
{
    if (filterStrings.isEmpty()) {
        return true;
    }

    uint ums = unmatchedStrings;
    int numStrings = filterStrings.count();

    foreach (const QString &candidate /*str*/, strings) {
//        QString candidate = str.simplified();

//        for (int i = 0; i < candidate.size(); ++i) {
//            if (candidate.at(i).decompositionTag() != QChar::NoDecomposition) {
//                candidate[i] = candidate[i].decomposition().at(0);
//            }
//        }

        for (int i = 0; i < numStrings; ++i) {
            if (candidate.contains(filterStrings.at(i), Qt::CaseInsensitive)) {
                ums &= ~(1<<i);
                if (0==ums) {
                    return true;
                }
            }
        }
    }

    return false;
}
//#include <QDebug>
bool ProxyModel::update(const QString &txt, const QString &genre, const void *f)
{
    QString text=txt.length()<2 ? QString() : txt;
//    qWarning() << "UPDATE" << txt << genre << (void *)f;
    if (text==origFilterText && genre==filterGenre && f==filter) {
//        qWarning() <<"NO CHANGE";
        return false;
    }

    bool wasEmpty=isEmpty();
    filterStrings = text.split(' ', QString::SkipEmptyParts, Qt::CaseInsensitive);
    unmatchedStrings = 0;
    const int n = qMin(filterStrings.count(), (int)sizeof(uint));
    for ( int i = 0; i < n; ++i ) {
        unmatchedStrings |= (1<<i);
    }

    origFilterText=text;
    filterGenre=genre;
    filter=f;

    if (text.isEmpty() && genre.isEmpty() && !filter) {
        if (filterEnabled) {
            filterEnabled=false;
            if (!wasEmpty) {
//                qWarning() << "INVALIDATE (empty from non)";
                invalidateFilter();
//                qWarning() << "DONE";
            }
            return true;
        }
    } else {
        filterEnabled=true;
//        qWarning() << "INVALIDATE (changed)";
        invalidateFilter();
//        qWarning() << "DONE";
        return true;
    }

    return false;
}

bool ProxyModel::isChildOfRoot(const QModelIndex &idx) const
{
    if (!rootIndex.isValid()) {
        return true;
    }

    QModelIndex i=idx;
    while(i.isValid()) {
        if (i==rootIndex) {
            return true;
        }
        i=i.parent();
    }
    return false;
}

void ProxyModel::sort(int column, Qt::SortOrder order)
{
    if (!isSorted || dynamicSortFilter()) {
        QSortFilterProxyModel::sort(column, order);
        isSorted=true;
    }
}
