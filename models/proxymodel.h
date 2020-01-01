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

#ifndef PROXYMODEL_H
#define PROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QStringList>
#include "mpd-interface/song.h"
#include "config.h"

class QMimeData;

class ProxyModel : public QSortFilterProxyModel
{
public:
    ProxyModel(QObject *parent) : QSortFilterProxyModel(parent), isSorted(false), filterEnabled(false), filter(nullptr) { }
    ~ProxyModel() override { }

    bool update(const QString &text);
    const void * filterItem() const { return filter; }
    void setFilterItem(void *f) { filter=f; }
    void setRootIndex(const QModelIndex &idx) { rootIndex=idx.isValid() ? mapToSource(idx) : idx; }
    bool isChildOfRoot(const QModelIndex &idx) const;
    bool isEmpty() const { return filterStrings.isEmpty() && nullptr==filter; }
    bool enabled() const { return filterEnabled; }
    const QString & filterText() const { return origFilterText; }
    void resort();
    void sort() { isSorted=false; sort(0); }
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QList<int> mapToSourceRows(const QModelIndexList &list) const;
    QModelIndex mapToSource(const QModelIndex &idx) const override { return QSortFilterProxyModel::mapToSource(idx); }
    QModelIndexList mapToSource(const QModelIndexList &list, bool leavesOnly=true) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    QModelIndexList leaves(const QModelIndexList &list) const;

protected:
    bool matchesFilter(const Song &s) const;
    bool matchesFilter(const QStringList &strings) const;

private:
    QModelIndexList leaves(const QModelIndex &idx) const;

protected:
    bool isSorted;
    bool filterEnabled;
    QModelIndex rootIndex;
    QString origFilterText;
    QStringList filterStrings;
    uint unmatchedStrings;
    const void *filter;
    quint16 yearFrom;
    quint16 yearTo;
};

#endif
