/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifndef ENABLE_UBUNTU
#include "settings.h"
#endif
#include <QMap>
#include <QString>
#include <QLatin1String>
#include <QChar>
#include <QMimeData>

bool ProxyModel::matchesFilter(const Song &s) const
{
    QStringList strings;

    strings << s.albumArtist();
    if (!s.albumartist.isEmpty() && s.albumartist!=s.artist) {
        strings << s.artist;
    }
    if (!s.composer.isEmpty() && s.composer!=s.artist && s.composer!=s.albumartist) {
        strings << s.composer;
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

    foreach (const QString &str, strings) {
        QString candidate = str.simplified();
        QString basic;
        bool useBasic=false;

        for (int i = 0; i < numStrings; ++i) {
            const QString &f=filterStrings.at(i);
            // Try to match string as entered by user...
            if (candidate.contains(f, Qt::CaseInsensitive)) {
                ums &= ~(1<<i);
                if (0==ums) {
                    return true;
                }
            } else {
                // Try converting string to basic - e.g. remove umlauts, etc...
                if (basic.isEmpty()) {
                    basic = candidate;
                    for (int i = 0; i < basic.size(); ++i) {
                        if (basic.at(i).decompositionTag() != QChar::NoDecomposition) {
                            basic[i] = basic[i].decomposition().at(0);
                            useBasic=true;
                        }
                    }
                }
                if (useBasic && basic.contains(f, Qt::CaseInsensitive)) {
                    ums &= ~(1<<i);
                    if (0==ums) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}
//#include <QDebug>
bool ProxyModel::update(const QString &txt, const QString &genre)
{
    QString text=txt.length()<2 ? QString() : txt;
//    qWarning() << "UPDATE" << txt << genre << (void *)f;
    if (text==origFilterText && genre==filterGenre) {
//        qWarning() <<"NO CHANGE";
        return false;
    }

    bool wasEmpty=isEmpty();
    filterStrings = text.split(' ', QString::SkipEmptyParts, Qt::CaseInsensitive);
    unmatchedStrings = 0;
    const int n = qMin(filterStrings.count(), (int)(sizeof(uint)*8));
    for ( int i = 0; i < n; ++i ) {
        unmatchedStrings |= (1<<i);
    }

    origFilterText=text;
    filterGenre=genre;

    if (text.isEmpty() && genre.isEmpty()) {
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

QList<int> ProxyModel::mapToSourceRows(const QModelIndexList &list) const
{
    QList<int> rows;
    foreach (const QModelIndex &idx, list) {
        rows.append(mapToSource(idx).row());
    }
    return rows;
}

QModelIndexList ProxyModel::mapToSource(const QModelIndexList &list, bool leavesOnly) const
{
    QModelIndexList mapped;
    if (leavesOnly) {
        QModelIndexList l=leaves(list);
        foreach (const QModelIndex &idx, l) {
            mapped.append(mapToSource(idx));
        }
    } else {
        foreach (const QModelIndex &idx, list) {
            mapped.append(mapToSource(idx));
        }
    }
    return mapped;
}

#ifndef ENABLE_UBUNTU
QMimeData * ProxyModel::mimeData(const QModelIndexList &indexes) const
{
    QModelIndexList nodes=Settings::self()->filteredOnly() ? leaves(indexes) : indexes;
    QModelIndexList sourceIndexes;
    foreach (const QModelIndex &idx, nodes) {
        sourceIndexes << mapToSource(idx);
    }
    return sourceModel()->mimeData(sourceIndexes);
}
#endif

QModelIndexList ProxyModel::leaves(const QModelIndexList &list) const
{
    QModelIndexList l;

    foreach (const QModelIndex &idx, list) {
        l+=leaves(idx);
    }
    return l;
}

QModelIndexList ProxyModel::leaves(const QModelIndex &idx) const
{
    QModelIndexList list;
    int rc=rowCount(idx);
    if (rc>0) {
        for (int i=0; i<rc; ++i) {
            list+=leaves(index(i, 0, idx));
        }
    } else {
        list+=idx;
    }
    return list;
}
