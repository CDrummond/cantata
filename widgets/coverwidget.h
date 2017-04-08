/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef COVERWIDGET_H
#define COVERWIDGET_H

#include <QWidget>
#include <QLabel>

class CoverLabel : public QLabel
{
    Q_OBJECT
public:
    CoverLabel(QWidget *p);

    bool event(QEvent *event);
    void paintEvent(QPaintEvent *);
    void updatePix();
    void deletePix();

private:
    bool pressed;
    QPixmap pix;
};

class CoverWidget : public QWidget
{
    Q_OBJECT

public:
    CoverWidget(QWidget *p);
    virtual ~CoverWidget();

    void setSize(int min);
    void setEnabled(bool e);

    void emitClicked() { emit clicked(); }

Q_SIGNALS:
    void clicked();

private Q_SLOTS:
    void coverImage(const QImage &);

private:
    CoverLabel *label;
};

#endif
