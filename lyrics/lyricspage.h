/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
// #include <QtCore/QScopedPointer>
#include "song.h"
#include "ui_lyricspage.h"

class UltimateLyricsProvider;
// class UltimateLyricsReader;
class UltimateLyricsProvider;

class LyricsPage : public QWidget, public Ui::LyricsPage
{
  Q_OBJECT

public:
    static const QLatin1String constExtension;

    LyricsPage(QWidget *parent);
    ~LyricsPage();

    void save();
    void setEnabledProviders(const QStringList &providerList);
    void update(const Song &song, bool force=false);
    const QList<UltimateLyricsProvider *> & getProviders() { return providers; }

Q_SIGNALS:
    void providersUpdated();

protected Q_SLOTS:
    void resultReady(int id, const QString &lyrics);
    void update();

private:
    UltimateLyricsProvider * providerByName(const QString &name) const;
    void getLyrics();

    /**
     * Reads the lyrics from the given filePath and updates
     * the UI with those lyrics.
     * 
     * @param filePath The path to the lyrics file which will be read.
     * 
     * @return Returns true if the file could be read; otherwise false.
     */
    bool setLyricsFromFile(const QString &filePath) const;

// private Q_SLOTS:
//     void ultimateLyricsParsed();

private:
//     QScopedPointer<UltimateLyricsReader> reader;
    QList<UltimateLyricsProvider *> providers;
    int currentProvider;
    int currentRequest;
    Song currentSong;
    QAction *refreshAction;
};

#endif
