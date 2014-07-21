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

#include "ratingwidget.h"
#include "support/icon.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QSvgRenderer>
#include <QApplication>
#include <QLabel>

const int RatingPainter::constNumStars;
const int RatingPainter::constNumPixmaps;

static const int constBorder=2;

RatingPainter::RatingPainter(int s)
    : starSz(s)
    , pixmapSize((starSz*constNumStars)+(constBorder*(constNumStars-1)), starSz)
    , col(QApplication::palette().text().color())
{
}

void RatingPainter::paint(QPainter *p, const QRect &r, int rating)
{
    if (rating<0 || rating>constNumStars) {
        return;
    }
    if (isNull()) {
        QSvgRenderer renderer;
        QFile f(":stars.svg");
        QByteArray bytes;
        if (f.open(QIODevice::ReadOnly)) {
            bytes=f.readAll();
        }
        if (!bytes.isEmpty()) {
            bytes.replace("#000", col.name().toLatin1());
        }
        renderer.load(bytes);

        for (int p=0; p<constNumPixmaps; ++p) {
            pixmaps[p]=QPixmap(pixmapSize);
            pixmaps[p].fill(Qt::transparent);
            QPainter painter(&(pixmaps[p]));
            for (int i=0; i<constNumStars; ++i) {
                renderer.render(&painter, i<p ? "on" : "off", QRectF(i*(starSz+constBorder), 0, starSz, starSz));
            }
        }
    }
    p->drawPixmap(r, pixmaps[rating]);
}

void RatingPainter::setColor(const QColor &c)
{
    if (col!=c) {
        col=c;
        if (!isNull()) {
            pixmaps[0]=QPixmap();
        }
    }
}

RatingWidget::RatingWidget(QWidget *parent)
    : QWidget(parent)
    , rp(Icon::stdSize(QApplication::fontMetrics().height()*0.9))
    , val(0)
    , hoverVal(-1)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setMouseTracking(true);
    setFixedSize(rp.size());
}

void RatingWidget::setValue(int v)
{
    val = v;
    update();
}

void RatingWidget::setColor(const QColor &c)
{
    if (c!=rp.color()) {
        rp.setColor(c);
        if (!rp.isNull()) {
            update();
        }
    }
}

void RatingWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e)
    QPainter p(this);
    if (!isEnabled()) {
        p.setOpacity(0.5);
    }
    rp.paint(&p, rect(), -1==hoverVal ? val : hoverVal);
}

void RatingWidget::mousePressEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    val = valueForPos(e->pos());
    emit valueChanged(val);
}

void RatingWidget::mouseMoveEvent(QMouseEvent *e)
{
    Q_UNUSED(e)
    hoverVal = valueForPos(e->pos());
    update();
}

void RatingWidget::leaveEvent(QEvent *e)
{
    Q_UNUSED(e)
    hoverVal = -1;
    update();
}

QRect RatingWidget::contentsRect() const
{
    const QRect &r=rect();
    const int width = rp.starSize() * RatingPainter::constNumStars;
    const int x = r.x() + (r.width() - width) / 2;

    return QRect(x, r.y(), width, r.height());
}

int RatingWidget::valueForPos(const QPoint &pos) const
{
    const QRect contents = contentsRect();
    const double raw = double(pos.x() - contents.left()) / contents.width();
    return (raw*RatingPainter::constNumStars)+0.5;
}
