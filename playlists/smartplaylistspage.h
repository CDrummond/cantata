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

#ifndef SMART_PLAYLISTS_PAGE_H
#define SMART_PLAYLISTS_PAGE_H

#include "widgets/singlepagewidget.h"
#include "playlistproxymodel.h"
#include "rulesplaylists.h"

class Action;
class QLabel;

class SmartPlaylistsPage : public SinglePageWidget
{
    Q_OBJECT

    struct Command {
        Command(const RulesPlaylists::Entry &e=RulesPlaylists::Entry(), int a=0, quint8 prio=0, bool dec=false, quint32 i=0)
            : playlist(e.name), action(a), priority(prio), decreasePriority(dec), includeUnrated(e.includeUnrated),
              ratingFrom(e.ratingFrom), ratingTo(e.ratingTo),
              minDuration(e.minDuration), maxDuration(e.maxDuration), maxAge(e.maxAge), numTracks(e.numTracks), order(e.order),
              orderAscending(e.orderAscending), id(i) { }
        bool isEmpty() const { return playlist.isEmpty(); }
        void clear() { playlist.clear(); includeRules.clear(); excludeRules.clear(); songs.clear(); toCheck.clear(); checking.clear(); }
        bool haveRating() const { return ratingFrom>=0 && ratingTo>0; }

        QString playlist;

        int action;
        quint8 priority;
        bool decreasePriority;

        QList<QByteArray> includeRules;
        QList<QByteArray> excludeRules;

        bool filterRating = false;
        bool fetchRatings = false;

        bool includeUnrated = false;
        int ratingFrom = 0;
        int ratingTo = 0;
        int minDuration = 0;
        int maxDuration = 0;
        int maxAge = 0;
        int numTracks = 0;
        RulesPlaylists::Order order = RulesPlaylists::Order_Random;
        bool orderAscending = true;

        quint32 id;

        QString checking;
        QSet<Song> songs;
        QStringList toCheck;
    };

public:
    SmartPlaylistsPage(QWidget *p);
    ~SmartPlaylistsPage() override;
    void setView(int) override { }

Q_SIGNALS:
    void search(const QByteArray &query, const QString &id);
    void getRating(const QString &file);
    void error(const QString &str);

private Q_SLOTS:
    void addNew();
    void edit();
    void remove();
    void headerClicked(int level);
    void searchResponse(const QString &id, const QList<Song> &songs);
    void rating(const QString &file, quint8 val);

private:
    void doSearch() override;
    void controlActions() override;
    void enableWidgets(bool enable);
    void filterCommand();
    void addSongsToPlayQueue();
    void addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority) override;

private:
    PlaylistProxyModel proxy;
    Action *addAction;
    Action *editAction;
    Action *removeAction;
    QList<QWidget *> controls;
    Command command;
};

#endif
