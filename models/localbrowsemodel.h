/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef LOCAL_BROWSE_MODEL_H
#define LOCAL_BROWSE_MODEL_H

#include "support/icon.h"
#include <QFileSystemModel>
#include <QSortFilterProxyModel>

class LocalBrowseModel : public QFileSystemModel
{
    Q_OBJECT

public:
    LocalBrowseModel(const QString &name, const QString &title, const QString &descr, const QIcon &icon, QObject *p);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QString name() const { return pathName; }
    QString title() const { return pathTitle; }
    QString descr() const { return pathDescr; }
    const QIcon & icon() const { return icn; }

private:
    QString pathName;
    QString pathTitle;
    QString pathDescr;
    QIcon icn;
};

class FileSystemProxyModel : public QSortFilterProxyModel
{
public:
    FileSystemProxyModel(LocalBrowseModel *p);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    LocalBrowseModel *m;
};

#endif
