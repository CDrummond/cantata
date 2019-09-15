/*
 * Cantata
 *
 * Copyright (c) 2011-2019 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SPINNER_H
#define SPINNER_H

#include <QWidget>
class QTimer;
class Spinner : public QWidget
{
    Q_OBJECT

public:
    Spinner(QObject *p, bool inMiddle=false);
    ~Spinner() override { }

    void setWidget(QWidget *widget);
    void start();
    void stop();
    void paintEvent(QPaintEvent *event) override;
    bool isActive() const { return active; }

private Q_SLOTS:
    void timeout();

private:
    void setPosition();

private:
    QTimer *timer;
    int space;
    int value;
    bool active;
    bool central;
    bool onView;
};

#endif
