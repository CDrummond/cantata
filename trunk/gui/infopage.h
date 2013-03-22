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

#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QWidget>
#include <QMap>
#include "song.h"
#include "textbrowser.h"

class Song;
class ComboBox;
class QNetworkReply;
class QIODevice;

class InfoPage : public QWidget
{
    Q_OBJECT

public:
    static const QLatin1String constCacheDir;
    static const QLatin1String constInfoExt;

    InfoPage(QWidget *parent);
    virtual ~InfoPage() { abort(); }

    void saveSettings();
    void update(const Song &s, bool force=false);
    void setBgndImageEnabled(bool e) { text->enableImage(e); }
    bool bgndImageEnabled() { return text->imageEnabled(); }

public Q_SLOTS:
    void artistImage(const Song &song, const QImage &img);

private Q_SLOTS:
    void handleReply();
    void setBio();

private:
    void requestBio();
    void abort();
    bool parseResponse(const QByteArray &resp);

private:
    TextBrowser *text;
    ComboBox *combo;
    QMap<int, QString> biographies;
    Song currentSong;
    QNetworkReply *currentJob;
};

#endif
