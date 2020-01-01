/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef VOLUME_CONTROL_H
#define VOLUME_CONTROL_H

#include <QWidget>

class VolumeSlider;
class SelectorLabel;
class QStackedWidget;

class VolumeControl : public QWidget
{
    Q_OBJECT

public:
    VolumeControl(QWidget *p);
    ~VolumeControl();

    void setColor(const QColor &col);
    void initActions();
    void setPageStep(int step);

private Q_SLOTS:
    void selectControl();
    void stateChanged();
    void itemSelected(int i);

private:
    SelectorLabel *label;
    QStackedWidget *stack;
    VolumeSlider *mpdVol;
    VolumeSlider *httpVol;
};

#endif
