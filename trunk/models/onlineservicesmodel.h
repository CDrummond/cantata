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
#ifndef ONLINE_SERVICES_MODEL_H
#define ONLINE_SERVICES_MODEL_H

#include <QSet>
#include "mpd-interface/song.h"
#include "config.h"
#include "gui/covers.h"
#include "multimusicmodel.h"
#include "musiclibraryproxymodel.h"

class Device;
class OnlineService;
class PodcastService;
class OnlineDevice;
class MusicLibraryItem;
class MusicLibraryItemPodcast;
class MusicLibraryItemPodcastEpisode;

class OnlineServicesModel : public MultiMusicModel
{
    Q_OBJECT

public:
    static QString constUdiPrefix;
    static OnlineServicesModel * self();

    struct Provider
    {
        Provider(const QString &n, const QIcon &i, const QString &k, bool h, bool c)
            : name(n), icon(i), key(k), hidden(h), configurable(c) { }
        QString name;
        QIcon icon;
        QString key;
        bool hidden;
        bool configurable;
    };

    OnlineServicesModel(QObject *parent = 0);
    ~OnlineServicesModel();
    QModelIndex serviceIndex(const OnlineService *srv) const;
    bool hasChildren(const QModelIndex &index) const;
    bool canFetchMore(const QModelIndex &index) const;
    void fetchMore(const QModelIndex &index);
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    void clear();
    OnlineService * service(const QString &name);
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    bool serviceIsBusy(const QString &id) const { return busyServices.contains(id); }
    void stop();
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void setSearch(const QString &serviceName, const QString &text);
    #ifdef ENABLE_DEVICES_SUPPORT
    Device *device(const QString &udi);
    #endif
    Action * configureAct() const { return configureAction; }
    Action * refreshAct() const { return refreshAction; }
    Action * subscribeAct() const { return subscribeAction; }
    Action * unSubscribeAct() const { return unSubscribeAction; }
    Action * refreshSubscriptionAct() const { return refreshSubscriptionAction; }
    bool subscribePodcast(const QUrl &url);
    void downloadPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);
    void deleteDownloadedPodcasts(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes);
    void setPodcastsAsListened(MusicLibraryItemPodcast *pod, const QList<MusicLibraryItemPodcastEpisode *> &episodes, bool listened);
    bool isDownloading() const;
    void cancelAll();
    bool isHidden(OnlineService *srv) { return hiddenServices.contains(srv); }

    void save();
    QList<Provider> getProviders() const;
    void setHiddenProviders(const QSet<QString> &prov);

public Q_SLOTS:
    void stateChanged(const QString &name, bool state);

private:
    OnlineService * addService(const QString &name, const QSet<QString> &hidden);
    void removeService(const QString &name);
    void updateItemMenu();
    void load();
    void setBusy(const QString &id, bool b);

private Q_SLOTS:
    void tooltipUpdated(QAction *act);
    void coverLoaded(const Song &song, int size);

Q_SIGNALS:
    void error(const QString &text);
    void podcastError(const QString &text);
    void updated(const QModelIndex &idx);
//    void needToSort();
    void busy(bool);
    void providersChanged();

private:
    bool enabled;
    OnlineDevice *dev;
    Action *configureAction;
    Action *refreshAction;
    Action *subscribeAction;
    Action *unSubscribeAction;
    Action *refreshSubscriptionAction;
    Action *searchAction;
    QSet<QString> busyServices;
    PodcastService *podcast;
    QList<OnlineService *> hiddenServices;
    friend class OnlineService;
};

#endif
