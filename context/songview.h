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

#ifndef SONG_VIEW_H
#define SONG_VIEW_H

#include <QWidget>
#include "view.h"
#include "config.h"

class UltimateLyricsProvider;
class QImage;
class Action;
class NetworkJob;
class QTimer;
class ContextEngine;

class SongView : public View
{
    Q_OBJECT

    enum Mode {
        Mode_Blank,
        Mode_Display
    };

    enum Pages {
        Page_Lyrics,
        Page_Information,
        Page_Metadata
    };

public:
    static const QLatin1String constLyricsDir;
    static const QLatin1String constExtension;
    static const QLatin1String constCacheDir;
    static const QLatin1String constInfoExt;

    SongView(QWidget *p);
    ~SongView() override;

    void update(const Song &s, bool force=false) override;
    void saveConfig();

Q_SIGNALS:
    void providersUpdated();

public Q_SLOTS:
    void downloadFinished();
    void lyricsReady(int, QString lyrics);
    void update();
    void search();
    void edit();
    void del();
    void showContextMenu(const QPoint &pos);
    void showInfoContextMenu(const QPoint &pos);

private Q_SLOTS:
    void toggleScroll();
    void songPosition();
    void scroll();
    void curentViewChanged();
    void refreshInfo();
    void infoSearchResponse(const QString &resp, const QString &lang);
    void abortInfoSearch();
    void showMoreInfo(const QUrl &url);

private:
    void loadLyrics();
    void loadLyricsFromFile();
    void loadInfo();
    void loadMetadata();
    void searchForInfo();
    void hideSpinner();
    void abort() override;
    QString mpdFileName() const;
    QString cacheFileName() const;
    void getLyrics();
    void setMode(Mode m);
    bool saveFile(const QString &fileName);

    /**
     * Reads the lyrics from the given filePath and updates
     * the UI with those lyrics.
     *
     * @param filePath The path to the lyrics file which will be read.
     *
     * @return Returns true if the file could be read; otherwise false.
     */
    bool setLyricsFromFile(const QString &filePath);

private:
    QTimer *scrollTimer;
    qint32 songPos;
    int currentProvider;
    int currentRequest;
    Action *scrollAction;
    Action *refreshAction;
    Action *searchAction;
    Action *editAction;
    Action *saveAction;
    Action *cancelEditAction;
    Action *delAction;
    Mode mode;
    QString lyricsFile;
    QString preEdit;
    NetworkJob *job;
    UltimateLyricsProvider *currentProv;

    bool lyricsNeedsUpdating;
    bool infoNeedsUpdating;
    bool metadataNeedsUpdating;
    Action *refreshInfoAction;
    Action *cancelInfoJobAction;
    ContextEngine *engine;
};

#endif
