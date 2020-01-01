/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "gui/settings.h"
#include <QMap>
#include <QString>
#include <QLatin1String>
#include <QChar>
#include <QMimeData>

bool ProxyModel::matchesFilter(const Song &s) const
{
    if (yearFrom>0 && yearTo>0 && (s.year<yearFrom || s.year>yearTo)) {
        return false;
    }

    QStringList strings;

    strings << s.albumArtist();
    if (!s.albumartist.isEmpty() && s.albumartist!=s.artist) {
        strings << s.artist;
    }
    if (!s.composer().isEmpty() && s.composer()!=s.artist && s.composer()!=s.albumartist) {
        strings << s.composer();
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

    for (const QString &str: strings) {
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

static const quint16 constMinYear=1500;
static const quint16 constMaxYear=2500; // 2500 (bit hopeful here :-) )

bool ProxyModel::update(const QString &txt)
{
    QString text=txt.length()<2 ? QString() : txt;
//    qWarning() << "UPDATE" << txt << (void *)f;
    if (text==origFilterText) {
//        qWarning() <<"NO CHANGE";
        return false;
    }

    bool wasEmpty=isEmpty();
    filterStrings.clear();
    yearFrom=yearTo=0;

    QStringList parts = text.split(' ', QString::SkipEmptyParts, Qt::CaseInsensitive);

    for (const auto &str: parts) {
        if (str.startsWith('#')) {
            QStringList parts=str.mid(1).split('-');
            if (1==parts.length()) {
                quint16 val=parts.at(0).simplified().toUInt();
                if (val>=constMinYear && val<=constMaxYear) {
                    yearFrom = yearTo = val;
                    continue;
                }
            } else if (2==parts.length()) {
                quint16 from=parts.at(0).simplified().toUInt();
                quint16 to=parts.at(1).simplified().toUInt();
                if (from>=constMinYear && from<=constMaxYear && to>=constMinYear && to<=constMaxYear) {
                    yearFrom=from;
                    yearTo=to;
                    continue;
                }
            }
        }
        filterStrings.append(str);
    }

    unmatchedStrings = 0;
    const int n = qMin(filterStrings.count(), (int)(sizeof(uint)*8));
    for ( int i = 0; i < n; ++i ) {
        unmatchedStrings |= (1<<i);
    }

    origFilterText=text;

    if (text.isEmpty()) {
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

void ProxyModel::resort()
{
    invalidate();
    sort();
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
    for (const QModelIndex &idx: list) {
        rows.append(mapToSource(idx).row());
    }
    return rows;
}

QModelIndexList ProxyModel::mapToSource(const QModelIndexList &list, bool leavesOnly) const
{
    QModelIndexList mapped;
    if (leavesOnly) {
        QModelIndexList l=leaves(list);
        for (const QModelIndex &idx: l) {
            mapped.append(mapToSource(idx));
        }
    } else {
        for (const QModelIndex &idx: list) {
            mapped.append(mapToSource(idx));
        }
    }
    return mapped;
}

QMimeData * ProxyModel::mimeData(const QModelIndexList &indexes) const
{
    QModelIndexList nodes=leaves(indexes);
    QModelIndexList sourceIndexes;
    for (const QModelIndex &idx: nodes) {
        sourceIndexes << mapToSource(idx);
    }
    return sourceModel()->mimeData(sourceIndexes);
}

QModelIndexList ProxyModel::leaves(const QModelIndexList &list) const
{
    QModelIndexList l;

    for (const QModelIndex &idx: list) {
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
