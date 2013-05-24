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
 
#ifndef CONTEXT_PAGE_H
#define CONTEXT_PAGE_H

#include <QWidget>
#include <QImage>
#include <QColor>

class Song;
class ArtistView;
class AlbumView;
class SongView;
class QNetworkReply;

class ContextPage : public QWidget
{
    Q_OBJECT

public:
    static const QLatin1String constCacheDir;

    ContextPage(QWidget *parent=0);

    void readConfig();
    void saveConfig();
    void useBackdrop(bool u);
    void useDarkBackground(bool u);
    void update(const Song &s);
    void showEvent(QShowEvent *e);
    void paintEvent(QPaintEvent *e);

Q_SIGNALS:
    void findArtist(const QString &artist);
    void findAlbum(const QString &artist, const QString &album);
    void playSong(const QString &file);

private Q_SLOTS:
    void searchResponse();
    void downloadResponse();

private:
    bool eventFilter(QObject *o, QEvent *e);
    void cancel();
    void updateBackdrop();
    void getBackdrop();
    QNetworkReply * getReply(QObject *obj);

private:
    QNetworkReply *job;
    bool drawBackdrop;
    bool darkBackground;
    QImage backdrop;
    QString currentArtist;
    QString updateArtist;
    ArtistView *artist;
    AlbumView *album;
    SongView *song;
    QColor appLinkColor;
};

#endif
