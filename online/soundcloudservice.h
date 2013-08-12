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

#ifndef SOUNDCLOUD_SERVICE_H
#define SOUNDCLOUD_SERVICE_H

#include "onlineservice.h"
#include <QLatin1String>

class SoundCloudService : public OnlineService
{
    Q_OBJECT

public:
    static const QLatin1String constName;

    SoundCloudService(MusicModel *m);
    ~SoundCloudService() { cancelAll(); }

    Song fixPath(const Song &orig, bool) const;
    void loadConfig() { }
    void saveConfig() { }
    void createLoader() { }
    bool canConfigure() const { return false; }
    bool canLoad() const { return false; }
    bool isSearchBased() const { return true; }
    QString currentSearchString() const { return currentSearch; }
    void setSearch(const QString &searchTerm);
    void cancelSearch() { cancelAll(); }
    void clear();
    bool isSearching() const { return 0!=job; }
    bool isFlat() const { return true; }

private:
    void cancelAll();

private Q_SLOTS:
    void jobFinished();

private:
    QNetworkReply *job;
    QString currentSearch;
};

#endif

