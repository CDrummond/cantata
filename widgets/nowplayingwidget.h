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

#ifndef NOWPLAYING_WIDGET_H
#define NOWPLAYING_WIDGET_H

#include <QWidget>
#include <QTime>
#include <QSlider>
#include "support/squeezedtextlabel.h"

class QTimer;
class QLabel;
class TimeLabel;
class RatingWidget;
struct Song;

class PosSlider : public QSlider
{
    Q_OBJECT
public:
    PosSlider(QWidget *p);
    ~PosSlider() override { }

    void updateStyleSheet(const QColor &col);
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *ev) override;
    void setRange(int min, int max);

Q_SIGNALS:
    void positionSet();
};

class NowPlayingWidget : public QWidget
{
    Q_OBJECT
public:
    NowPlayingWidget(QWidget *p);
    ~NowPlayingWidget() override { }
    void update(const Song &song);
    void startTimer();
    void stopTimer();
    void setValue(int v);
    void setRange(int min, int max);
    void clearTimes();
    int value() const;
    void readConfig();
    void saveConfig();
    void setEnabled(bool e) { slider->setEnabled(e); }
    bool isEnabled() const { return slider->isEnabled(); }
    void initColors();
    QColor textColor() const { return track->palette().windowText().color(); }
    void resizeEvent(QResizeEvent *ev) override;

Q_SIGNALS:
    void sliderReleased();

    void mpdPoll();
    void setRating(const QString &file, quint8 r);

public Q_SLOTS:
    void rating(const QString &file, quint8 r);

private Q_SLOTS:
    void updateTimes();
    void updatePos();
    void pressed();
    void released();
    void setRating(int v);
    void updateInfo();
    void copyInfo();

private:
    void controlWidgets();

private:
    SqueezedTextLabel *track;
    SqueezedTextLabel *artist;
    QLabel *infoLabel;
    TimeLabel *time;
    PosSlider *slider;
    RatingWidget *ratingWidget;
    QTimer *timer;
    QTime startTime;
    QString currentSongFile;
    int lastVal;
    int pollCount;
};

#endif
