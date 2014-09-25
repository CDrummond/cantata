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

#ifndef MUSIC_LIBRARY_ITEM_PODCAST_H
#define MUSIC_LIBRARY_ITEM_PODCAST_H

#include <QList>
#include <QVariant>
#include <QSet>
#include <QUrl>
#include "musiclibraryitem.h"
#include "musiclibraryitemsong.h"
#include "mpd/song.h"

class QPixmap;
class QNetworkReply;
class MusicLibraryItemPodcastEpisode;

class MusicLibraryItemPodcast : public MusicLibraryItemContainer
{
public:
    enum RssStatus {
        Loaded,
        FailedToParse,
        VideoPodcast
    };

    static const QString constExt;
    static const QString constDir;

    static bool lessThan(const MusicLibraryItem *a, const MusicLibraryItem *b) {
        return a->data().localeAwareCompare(b->data())<0;
    }

    MusicLibraryItemPodcast(const QString &fileName, MusicLibraryItemContainer *parent);
    virtual ~MusicLibraryItemPodcast() { }

    bool load();
    RssStatus loadRss(QNetworkReply *dev);
    bool save();
    void remove(int row);
    void remove(MusicLibraryItemSong *i);
    Type itemType() const { return Type_Podcast; }
    const QUrl & imageUrl() const { return m_imageUrl; }
    void setImageUrl(const QString &u) { m_imageUrl=u; }
    const QUrl & rssUrl() const { return m_rssUrl; }
    void removeFiles();
    void setUnplayedCount();
    quint32 unplayedEpisodes() const { return m_unplayedEpisodeCount; }
    void setPlayed(MusicLibraryItemSong *song, bool played=true);
    void addAll(const QList<MusicLibraryItemPodcastEpisode *> &others);
    MusicLibraryItemPodcastEpisode * getEpisode(const QString &file) const;
    Song coverSong() const;

private:
    void updateStats();

private:
    mutable bool m_coverRequested;
    QUrl m_imageUrl;
    QUrl m_rssUrl;
    QString m_fileName;
    QString m_imageFile;
    quint32 m_unplayedEpisodeCount;
};

class MusicLibraryItemPodcastEpisode : public MusicLibraryItemSong
{
public:
    enum DownloadState {
        NotDownloading = -1,
        QueuedForDownload = -2
    };

    MusicLibraryItemPodcastEpisode(const Song &s, MusicLibraryItemContainer *parent)
        : MusicLibraryItemSong(s, parent), downloadProg(NotDownloading) { }
    virtual ~MusicLibraryItemPodcastEpisode() { }

    const QString & published();
    QString localPath() { return m_song.podcastLocalPath(); }
    void setLocalPath(const QString &l) { m_song.setPodcastLocalPath(l); }
    void setDownloadProgress(int prog) { downloadProg=prog; }
    int downloadProgress() const { return downloadProg; }

private:
    QString publishedDate;
    QString local;
    int downloadProg;
};

#endif
