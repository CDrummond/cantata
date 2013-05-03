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

#ifndef LYRICSPAGE_H
#define LYRICSPAGE_H

#include <QWidget>
#include "song.h"
#include "ui_lyricspage.h"
#include "textbrowser.h"

class UltimateLyricsProvider;
class UltimateLyricsProvider;
class QImage;
class Action;
class QNetworkReply;

class LyricsPage : public QWidget, public Ui::LyricsPage
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

    LyricsPage(QWidget *p);
    ~LyricsPage();

    void saveSettings();
    void update(const Song &song, bool force=false);
    void setBgndImageEnabled(bool e) { text->enableImage(e); }
    bool bgndImageEnabled() { return text->imageEnabled(); }
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void providersUpdated();

public Q_SLOTS:
    void setImage(const QImage &img) { text->setImage(img); }

protected Q_SLOTS:
    void downloadFinished();
    void lyricsReady(int, QString lyrics);
    void update();
    void search();
    void edit();
    void save();
    void cancel();
    void del();

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
    bool setLyricsFromFile(const QString &filePath) const;

private:
    bool needToUpdate;
    int currentProvider;
    int currentRequest;
    Song currentSong;
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
