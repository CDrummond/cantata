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

#ifndef MPD_SEARCH_MODEL_H
#define MPD_SEARCH_MODEL_H

#include "searchmodel.h"

class MpdSearchModel : public SearchModel
{
    Q_OBJECT

public:
    MpdSearchModel(QObject *parent = nullptr);
    ~MpdSearchModel() override;

    QVariant data(const QModelIndex &index, int role) const override;
    void clear() override;
    void search(const QString &key, const QString &value) override;

Q_SIGNALS:
    void search(const QString &field, const QString &value, int id);
    void getRating(const QString &file) const;

private Q_SLOTS:
    void searchFinished(int id, const QList<Song> &result);
    void coverLoaded(const Song &song, int s);
    void ratingResult(const QString &file, quint8 r);

private:
    void clearItems();
    const Song * toSong(const QModelIndex &index) const { return index.isValid() ? static_cast<const Song *>(index.internalPointer()) : nullptr; }

private:
    int currentId;
};

#endif
