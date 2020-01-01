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

#ifndef VOLUMESLIDER_H
#define VOLUMESLIDER_H

#include <QSlider>
#include <QColor>

class QPixmap;
class QMenu;
class Action;
class QAction;

class VolumeSlider : public QSlider
{
    Q_OBJECT

public:
    static QColor clampColor(const QColor &col);

    VolumeSlider(bool isMpd, QWidget *p=nullptr);
    ~VolumeSlider() override { }

    void setActive(bool a) { isActive=a; }
    void initActions();
    void setColor(const QColor &col);
    void paintEvent(QPaintEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void contextMenuEvent(QContextMenuEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

Q_SIGNALS:
    void stateChanged();
    void toggleMute();

private Q_SLOTS:
    void muteToggled();
    void updateStatus();
    void increaseVolume();
    void decreaseVolume();

private:
    void generatePixmaps();
    QPixmap generatePixmap(bool filled);

private:
    bool isMpdVol;
    bool isActive;
    int lineWidth;
    bool down;
    QColor textCol;
    QPixmap pixmaps[2];
    Action *muteAction;
    QAction *muteMenuAction;
    QMenu *menu;
};

#endif
