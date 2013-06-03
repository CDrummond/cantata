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

#ifndef SONG_VIEW_H
#define SONG_VIEW_H

#include <QWidget>
#include "view.h"

class UltimateLyricsProvider;
class QImage;
class Action;
class QNetworkReply;

class SongView : public View
{
  Q_OBJECT

    enum Mode {
        Mode_Blank,
        Mode_Display,
        Mode_Edit
    };

public:
    static const QLatin1String constLyricsDir;
    static const QLatin1String constExtension;

    SongView(QWidget *p);
    ~SongView();

    void update(const Song &song, bool force=false);

Q_SIGNALS:
    void providersUpdated();

public Q_SLOTS:
    void downloadFinished();
    void lyricsReady(int, QString lyrics);
    void update();
    void search();
    void edit();
    void save();
    void cancel();
    void del();
    void showContextMenu(const QPoint &pos);

private:
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
    int currentProvider;
    int currentRequest;
    Action *refreshAction;
    Action *searchAction;
    Action *editAction;
    Action *saveAction;
    Action *cancelAction;
    Action *delAction;
    Mode mode;
    QString lyricsFile;
    QString preEdit;
    QNetworkReply *job;
};

#endif
