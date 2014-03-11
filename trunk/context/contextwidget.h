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
 
#ifndef CONTEXT_WIDGET_H
#define CONTEXT_WIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QPropertyAnimation>
#include <QSplitter>
#include "song.h"

#ifndef SCALE_CONTEXT_BGND
#define SCALE_CONTEXT_BGND
#endif

class ArtistView;
class AlbumView;
class SongView;
class BackdropCreator;
class NetworkJob;
class QStackedWidget;
class QComboBox;
class QImage;
class QToolButton;
class QButtonGroup;
class QWheelEvent;

class ViewSelector : public QWidget
{
    Q_OBJECT
public:
    ViewSelector(QWidget *p);
    virtual ~ViewSelector() { }
    void addItem(const QString &label, const QVariant &data);
    QVariant itemData(int index) const;
    int count() { return buttons.count(); }
    int currentIndex() const;
    void setCurrentIndex(int index);

private:
    void wheelEvent(QWheelEvent *ev);
    void paintEvent(QPaintEvent *);

private Q_SLOTS:
    void buttonActivated();

Q_SIGNALS:
    void activated(int);

private:
    QButtonGroup *group;
    QList<QToolButton *> buttons;
};

class ThinSplitter : public QSplitter
{
    Q_OBJECT
public:
    ThinSplitter(QWidget *parent);
    QSplitterHandle *createHandle();

public Q_SLOTS:
    void reset();

private:
    QAction *resetAct;
};

class ContextWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float fade READ fade WRITE setFade)

public:
    static void enableDebug();

    static const QLatin1String constCacheDir;
    static const QLatin1String constFanArtApiKey;

    ContextWidget(QWidget *parent=0);

    void readConfig();
    void saveConfig();
    void useDarkBackground(bool u);
    void update(const Song &s);
    void showEvent(QShowEvent *e);
    void paintEvent(QPaintEvent *e);
    float fade() { return fadeValue; }
    void setFade(float value);
    void updateImage(QImage img, bool created=false);
    void search();

Q_SIGNALS:
    void findArtist(const QString &artist);
    void findAlbum(const QString &artist, const QString &album);
    void playSong(const QString &file);
    void createBackdrop(const QString &artist, const QList<Song> &songs);

private Q_SLOTS:
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
    void updateBackdrop(bool force=false);
    void getBackdrop();
    void getFanArtBackdrop();
    void getMusicbrainzId(const QString &artist);
    void getDiscoGsImage();
    void createBackdrop();
    void resizeBackdrop();
    NetworkJob * getReply(QObject *obj);

private:
    NetworkJob *job;
    bool alwaysCollapsed;
    int backdropType;
    int backdropOpacity;
    int backdropBlur;
    QString customBackdropFile;
    bool darkBackground;
    bool useFanArt;
    bool albumCoverBackdrop;
    bool oldIsAlbumCoverBackdrop;
    Song currentSong;
    #ifdef SCALE_CONTEXT_BGND
    QImage currentImage;
    #endif
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
    ViewSelector *viewSelector;
    BackdropCreator *creator;
    QSet<QString> backdropAlbums;
    #ifndef SCALE_CONTEXT_BGND
    QSize minBackdropSize;
    QSize maxBackdropSize;
    #endif
    QList<QString> artistsCreatedBackdropsFor;
};

#endif
