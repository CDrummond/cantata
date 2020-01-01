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
 
#ifndef CONTEXT_WIDGET_H
#define CONTEXT_WIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QPropertyAnimation>
#include <QSplitter>
#include "mpd-interface/song.h"

class ArtistView;
class AlbumView;
class SongView;
class NetworkJob;
class QStackedWidget;
class QComboBox;
class QImage;
class QToolButton;
class QButtonGroup;
class QWheelEvent;
class OnlineView;

class ViewSelector : public QWidget
{
    Q_OBJECT
public:
    ViewSelector(QWidget *p);
    ~ViewSelector() override { }
    void addItem(const QString &label, const QVariant &data);
    QVariant itemData(int index) const;
    int count() { return buttons.count(); }
    int currentIndex() const;
    void setCurrentIndex(int index);

private:
    void wheelEvent(QWheelEvent *ev) override;
    void paintEvent(QPaintEvent *) override;

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
    QSplitterHandle *createHandle() override;

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

    static const QLatin1String constBackdropFileName;
    static const QLatin1String constCacheDir;

    ContextWidget(QWidget *parent=nullptr);

    void readConfig();
    void saveConfig();
    void useDarkBackground(bool u);
    void update(const Song &s);
    void showEvent(QShowEvent *e) override;
    void paintEvent(QPaintEvent *e) override;
    float fade() { return fadeValue; }
    void setFade(float value);
    void updateImage(QImage img);
    void search();

private:
    void updatePalette();

Q_SIGNALS:
    void findArtist(const QString &artist);
    void findAlbum(const QString &artist, const QString &album);
    void playSong(const QString &file);

private Q_SLOTS:
    void musicbrainzResponse();
    void fanArtResponse();
    void downloadResponse();

private:
    void setWide(bool w);
    void resizeEvent(QResizeEvent *e) override;
    void cancel();
    void updateBackdrop(bool force=false);
    void getBackdrop();
    void getFanArtBackdrop();
    void getMusicbrainzId(const QString &artist);
    void createBackdrop();
    void resizeBackdrop();
    NetworkJob * getReply(QObject *obj);

private:
    bool shown;
    NetworkJob *job;
    bool alwaysCollapsed;
    int backdropType;
    int backdropOpacity;
    int backdropBlur;
    QString customBackdropFile;
    bool darkBackground;
    Song currentSong;
    QImage currentImage;
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
    QStackedWidget *mainStack;
    QStackedWidget *stack;
    QWidget *standardContext;
    OnlineView *onlineContext;
    ThinSplitter *splitter;
    ViewSelector *viewSelector;
};

#endif
