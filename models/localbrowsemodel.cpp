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

#include "localbrowsemodel.h"
#include "roles.h"
#include "playqueuemodel.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"

FileSystemProxyModel::FileSystemProxyModel(LocalBrowseModel *p)
    : QSortFilterProxyModel(p)
    , m(p)
{
    setSourceModel(p);
}

bool FileSystemProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    QFileInfo info = m->fileInfo(index);
    if (info.isDir()) {
        return true;
    }
    QString name = info.fileName();
    int pos = name.lastIndexOf(".");
    if (-1==pos) {
        return false;
    }
    return PlayQueueModel::constFileExtensions.contains(name.mid(pos+1).toLower());
}

bool FileSystemProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QFileInfo l = m->fileInfo(left);
    QFileInfo r = m->fileInfo(right);

    if (l.isDir() && !r.isDir()) {
        return true;
    }
    if (!l.isDir() && r.isDir()) {
        return false;
    }
    return l.fileName().toLower().localeAwareCompare(r.fileName().toLower())<0;
}

LocalBrowseModel::LocalBrowseModel(const QString &name, const QString &title, const QString &descr, const QIcon &icon, QObject *p)
    : QFileSystemModel(p)
    , pathName(name)
    , pathTitle(title)
    , pathDescr(descr)
    , icn(icon)
{
    setFilter(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::Drives);
}

QVariant LocalBrowseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || fileInfo(index).absoluteFilePath()==rootPath()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return descr();
        case Qt::DecorationRole:
            return icon();
        }
    }

    switch (role) {
    case Qt::DecorationRole: {
        QFileInfo info = fileInfo(index);
        return info.isDir()
                ? Icons::self()->folderListIcon
                : MPDConnection::isPlaylist(info.fileName())
                    ? Icons::self()->playlistListIcon
                    : Icons::self()->audioListIcon;
    }
    case Cantata::Role_TitleText:
        return QFileSystemModel::data(index, Qt::DisplayRole);
    case Cantata::Role_SubText: {
        QFileInfo info = fileInfo(index);
        return info.isDir() ? QString() : Utils::formatByteSize(info.size());
    }
    case Cantata::Role_Actions: {
        QVariant v;
        v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StdActions::self()->appendToPlayQueueAction);
        return v;
    }
    default:
        return QFileSystemModel::data(index, role);
    }
}

#include "moc_localbrowsemodel.cpp"
