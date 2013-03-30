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
#ifndef ONLINE_SERVICES_MODEL_H
#define ONLINE_SERVICES_MODEL_H

#include <QSet>
#include "song.h"
#include "config.h"
#include "actionmodel.h"
#include "musiclibraryproxymodel.h"

class Device;
class OnlineService;
class OnlineDevice;
class MusicLibraryItem;

class OnlineServicesModel : public ActionModel
{
    Q_OBJECT

public:
    static QString constUdiPrefix;
    static OnlineServicesModel * self();

    OnlineServicesModel(QObject *parent = 0);
    ~OnlineServicesModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes) const;
    QList<Song> songs(const QModelIndexList &indexes) const;
    void clear(bool clearConfig=true);
    OnlineService * service(const QString &name);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    void stop();
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void toggleGrouping();
    const QSet<QString> & genres() { return srvGenres; }
    void createService(const QString &name);
    Device *device(const QString &udi);
    void removeService(const QString &name, bool fullRemove=true);
    Action * configureAct() const { return configureAction; }
    Action * refreshAct() const { return refreshAction; }
    Action * connectAct() const { return connectAction; }
    Action * disconnectAct() const { return disconnectAction; }

public Q_SLOTS:
    void setArtistImage(const Song &song, const QImage &img);
    void setCover(const Song &song, const QImage &img, const QString &fileName);
    void stateChanged(const QString &name, bool state);

private:
    OnlineService * addService(const QString &name);
    void updateItemMenu();
    void load();
    int indexOf(const QString &name);
    void updateGenres();

Q_SIGNALS:
    void updateGenres(const QSet<QString> &genres);
    void error(const QString &text);
    void updated(const QModelIndex &idx);

private:
    QList<OnlineService *> services;
    QSet<QString> srvGenres;
    bool enabled;
    OnlineDevice *dev;
    Action *configureAction;
    Action *refreshAction;
    Action *connectAction;
    Action *disconnectAction;

    friend class OnlineService;
};

#endif
