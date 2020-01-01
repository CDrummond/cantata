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

#ifndef STREAM_SEARCH_MODEL_H
#define STREAM_SEARCH_MODEL_H

#include "streamsmodel.h"
#include <QList>
#include <QMap>
#include <QIcon>
#include <QDateTime>

class NetworkJob;
class QXmlStreamReader;
class QIODevice;

class StreamSearchModel : public ActionModel
{
    Q_OBJECT

public:
    enum Category {
        // DO NOT CHANGE ORDER!
        TuneIn,
        ShoutCast,
        Dirble,
        CommunityRadio,
        NumCategories
    };

    StreamSearchModel(QObject *parent = nullptr);
    ~StreamSearchModel() override;
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override;
    QVariant data(const QModelIndex &, int) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool hasChildren(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &index) const override;
    void fetchMore(const QModelIndex &index) override;

    QStringList filenames(const QModelIndexList &indexes, bool addPrefix) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    void clear();
    void search(const QString &searchTerm, bool stationsOnly);
    void cancelAll();
 
Q_SIGNALS:
    void loading();
    void loaded();
    void error(const QString &msg);

private Q_SLOTS:
    void jobFinished();

private:
    QList<StreamsModel::Item *> getStreams(StreamsModel::CategoryItem *cat);
    StreamsModel::Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<StreamsModel::Item*>(index.internalPointer()) : root; }
    QList<StreamsModel::Item *> parseRadioTimeResponse(QIODevice *dev, StreamsModel::CategoryItem *cat);
    StreamsModel::Item * parseRadioTimeEntry(QXmlStreamReader &doc, StreamsModel::CategoryItem *parent);

private:
    Category category;
    QMap<NetworkJob *, StreamsModel::CategoryItem *> jobs;
    StreamsModel::CategoryItem *root;
    QString currentSearch;
    QIcon icon;
};

#endif
