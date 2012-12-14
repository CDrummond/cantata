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

#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include <QtGui/QSlider>
#include <QtCore/QTime>
class QTimer;

class TimeSlider : public QSlider
{
    Q_OBJECT
public:
    TimeSlider(QWidget *p);
    virtual ~TimeSlider() {
    }
    void startTimer();
    void stopTimer();
    void setValue(int v);

private Q_SLOTS:
    void updatePos();
    void pressed();
    void released();

private:
    QTimer *timer;
    QTime startTime;
    int lastVal;
};

#endif
