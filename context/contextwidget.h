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
 
#ifndef CONTEXT_WIDGET_H
#define CONTEXT_WIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QPropertyAnimation>
#include <QSplitter>
#include "song.h"

class ArtistView;
class AlbumView;
class SongView;
class BackdropCreator;
class QNetworkReply;
class QStackedWidget;
class QComboBox;
class QImage;

class ThinSplitter : public QSplitter
{
    Q_OBJECT
public:
    ThinSplitter(QWidget *parent);
    QSplitterHandle *createHandle();

public Q_SLOTS:
    void reset();
};

class ContextWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float fade READ fade WRITE setFade)

public:
    static void enableDebug();

    static const QLatin1String constCacheDir;
//    static const QLatin1String constHtbApiKey;
    static const QLatin1String constFanArtApiKey;

    ContextWidget(QWidget *parent=0);

    void readConfig();
    void saveConfig();
    void useBackdrop(bool u);
    void useDarkBackground(bool u);
    void update(const Song &s);
    void showEvent(QShowEvent *e);
    void paintEvent(QPaintEvent *e);
    float fade() { return fadeValue; }
    void setFade(float value);
    void updateImage(const QImage &img, bool created=false);
    void search();

Q_SIGNALS:
    void findArtist(const QString &artist);
    void findAlbum(const QString &artist, const QString &album);
    void playSong(const QString &file);
    void createBackdrop(const QString &artist, const QList<Song> &songs);

private Q_SLOTS:
//    void htBackdropsResponse();
    void musicbrainzResponse();
    void fanArtResponse();
    void discoGsResponse();
    void downloadResponse();
    void backdropCreated(const QString &artist, const QImage &img);

private:
    void setZoom();
    void setWide(bool w);
    void resizeEvent(QResizeEvent *e);
    bool eventFilter(QObject *o, QEvent *e);
    void cancel();
    void updateBackdrop();
    void getBackdrop();
//    void getHtBackdrop();
    void getFanArtBackdrop();
    void getDiscoGsImage();
    void createBackdrop();
    QNetworkReply * getReply(QObject *obj);

private:
    QNetworkReply *job;
    bool drawBackdrop;
    bool darkBackground;
//    bool useHtBackdrops;
    bool useFanArt;
    QPixmap oldBackdrop;
    QPixmap currentBackdrop;
    QString currentArtist;
    QString updateArtist;
    ArtistView *artist;
    AlbumView *album;
    SongView *song;
    QColor appLinkColor;
    double fadeValue;
    QPropertyAnimation animator;
    int minWidth;
    bool isWide;
    QStackedWidget *stack;
    ThinSplitter *splitter;
    QComboBox *viewCombo;
    BackdropCreator *creator;
//    QString backdropText;
    QSet<QString> backdropAlbums;
    QSize minBackdropSize;
    QList<QString> artistsCreatedBackdropsFor;
};

#endif
