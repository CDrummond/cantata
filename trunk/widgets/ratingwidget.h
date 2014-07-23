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

#ifndef RATINGWIDGET_H
#define RATINGWIDGET_H

#include <QWidget>
#include <QPixmap>

class RatingPainter
{
public:
    static const int constNumStars=5;
    static const int constNumPixmaps=constNumStars+1;

    RatingPainter(int s);

    void paint(QPainter *p, const QRect &r, int rating);
    int starSize() const { return starSz; }
    void setColor(const QColor &c);
    const QColor & color() const { return col; }
    const QSize & size() const { return pixmapSize; }
    bool isNull() const { return pixmaps[0].isNull(); }

private:
    int starSz;
    QSize pixmapSize;
    QColor col;
    QPixmap pixmaps[constNumPixmaps];
};

class RatingWidget : public QWidget
{
    Q_OBJECT

public:

    RatingWidget(QWidget *parent = 0);

    QSize sizeHint() const { return rp.size(); }
    int value() const { return val; }
    void setValue(int v);
    void setColor(const QColor &c);
    void setShowZeroForNull(bool s) { showZeroForNull=s; }

Q_SIGNALS:
    void valueChanged(int v);

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void leaveEvent(QEvent *e);

private:
    QRect contentsRect() const;
    int valueForPos(const QPoint &pos) const;
    QColor getColor() const;

private:
    RatingPainter rp;
    int val;
    int hoverVal;
    bool showZeroForNull;
};

#endif
