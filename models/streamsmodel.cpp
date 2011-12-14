/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QModelIndex>
#include <QtCore/QDataStream>
#include <QtCore/QTextStream>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include <QtGui/QIcon>
#include "streamsmodel.h"
#include "settings.h"
#include "playlisttablemodel.h"
#include "config.h"

static QString configDir()
{
    QString env = qgetenv("XDG_CONFIG_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.config/" : env) + QLatin1String("/"PACKAGE_NAME"/");
    QDir d(dir);
    return d.exists() || d.mkpath(dir) ? QDir::toNativeSeparators(dir) : QString();
}

static QString getInternalFile()
{
    return configDir()+"streams.xml";
}

StreamsModel::StreamsModel()
    : QAbstractListModel(0)
    , modified(false)
    , timer(0)
{
}

StreamsModel::~StreamsModel()
{
}

QVariant StreamsModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamsModel::rowCount(const QModelIndex &) const
{
    return items.size();
}

QModelIndex StreamsModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if(row<items.count())
        return createIndex(row, column, (void *)&items.at(row));

    return QModelIndex();
}

QVariant StreamsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (index.row() >= items.size()) {
        return QVariant();
    }

    switch(role) {
    case Qt::DisplayRole: return items.at(index.row()).name;
    case Qt::ToolTipRole: return items.at(index.row()).url;
    case Qt::UserRole:    return items.at(index.row()).favorite;
    case Qt::DecorationRole:
        if (items.at(index.row()).favorite) {
            return QIcon::fromTheme("emblem-favorite");
        }
    default: break;
    }

    return QVariant();
}

static const QLatin1String constNameQuery("CantataStreamName");

void StreamsModel::reload()
{
    beginResetModel();
    items.clear();
    itemMap.clear();
    load(getInternalFile(), true);
    endResetModel();
}

void StreamsModel::save(bool force)
{
    if (force) {
        if (timer) {
            timer->stop();
        }
        persist();
    } else {
        if (!timer) {
            timer=new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(persist()));
        }
        timer->start(30*1000);
    }
}

bool StreamsModel::load(const QString &filename, bool isInternal)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument doc;
    bool haveInserted=false;
    doc.setContent(&file);
    QDomElement root = doc.documentElement();

    if ("cantata" == root.tagName() && root.hasAttribute("version") && "1.0" == root.attribute("version")) {
        QDomElement stream = root.firstChildElement("stream");
        while (!stream.isNull()) {
            if (stream.hasAttribute("name") && stream.hasAttribute("url")) {
                QString name=stream.attribute("name");
                QString origName=name;
                QUrl url=QUrl(stream.attribute("url"));
                bool fav=stream.hasAttribute("favorite") ? stream.attribute("favorite")=="true" : false;

                if (!entryExists(QString(), url)) {
                    int i=1;
                    for (; i<100 && entryExists(name); ++i) {
                        name=origName+QLatin1String("_")+QString::number(i);
                    }

                    if (i<100) {
                        if (!haveInserted) {
                            haveInserted=true;
                        }
                        if (!isInternal) {
                            beginInsertRows(QModelIndex(), items.count(), items.count());
                        }
                        items.append(Stream(name, url, fav));
                        itemMap.insert(url.toString(), name);
                        if (!isInternal) {
                            endInsertRows();
                        }
                    }
                }
            }
            stream=stream.nextSiblingElement("stream");
        }
    }

    if (haveInserted && !isInternal) {
        save();
    }

    return haveInserted;
}

bool StreamsModel::save(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QDomDocument doc;
    QDomElement root = doc.createElement("cantata");
    root.setAttribute("version", "1.0");
    doc.appendChild(root);
    foreach (const Stream &s, items) {
        QDomElement stream = doc.createElement("stream");
        stream.setAttribute("name", s.name);
        stream.setAttribute("url", s.url.toString());
        if (s.favorite) {
            stream.setAttribute("favorite", "true");
        }
        root.appendChild(stream);
    }

    QTextStream(&file) << doc.toString();
    return true;
}

bool StreamsModel::add(const QString &name, const QString &url, bool fav)
{
    QUrl u(url);

    if (entryExists(name, url)) {
        return false;
    }

    beginInsertRows(QModelIndex(), items.count(), items.count());
    items.append(Stream(name, u, fav));
    itemMap.insert(url, name);
    endInsertRows();
    modified=true;
    save();
    Settings::self()->save();
    return true;
}

void StreamsModel::edit(const QModelIndex &index, const QString &name, const QString &url, bool fav)
{
    QUrl u(url);
    int row=index.row();

    if (row<items.count()) {
        Stream old=items.at(row);
        items[index.row()]=Stream(name, u, fav);
        itemMap.insert(url, name);
        emit dataChanged(index, index);
        modified=true;
        save();
        Settings::self()->save();
    }
}

void StreamsModel::remove(const QModelIndex &index)
{
    int row=index.row();

    if (row<items.count()) {
        Stream old=items.at(row);
        beginRemoveRows(QModelIndex(), row, row);
        items.removeAt(index.row());
        itemMap.remove(old.url.toString());
        endRemoveRows();
        save();
        Settings::self()->save();
    }
}

QString StreamsModel::name(const QString &url)
{
    QHash<QString, QString>::ConstIterator it=itemMap.find(url);

    return it==itemMap.end() ? QString() : it.value();
}

bool StreamsModel::entryExists(const QString &name, const QUrl &url)
{
    foreach (const Stream &s, items) {
        if ( (!name.isEmpty() && s.name==name) || (!url.isEmpty() && s.url==url)) {
            return true;
        }
    }

    return false;
}

Qt::ItemFlags StreamsModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::NoItemFlags;
}

QMimeData * StreamsModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QStringList filenames;

    foreach(QModelIndex index, indexes) {
        if (index.row()<items.count()) {
            filenames << items.at(index.row()).url.toString();
        }
    }

    for (int i = filenames.size() - 1; i >= 0; i--) {
        stream << filenames.at(i);
    }

    mimeData->setData(PlaylistTableModel::constFileNameMimeType, encodedData);
    return mimeData;
}

void StreamsModel::persist()
{
    if (modified) {
        save(getInternalFile());
        modified=false;
    }
}

void StreamsModel::mark(const QList<int> &rows, bool f)
{
    emit layoutAboutToBeChanged();
    foreach (int r, rows) {
        if (items[r].favorite!=f) {
            items[r].favorite=f;
            QModelIndex idx=index(0, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
    }
    emit layoutChanged();
    modified=true;
}
