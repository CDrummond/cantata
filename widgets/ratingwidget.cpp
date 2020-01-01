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

#include "ratingwidget.h"
#include "support/icon.h"
#include "mpd-interface/song.h"
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QSvgRenderer>
#include <QApplication>
#include <QLabel>

static bool allowHalfStars=true; // TODO: Make this configurable???
static const int constNumStars=Song::Rating_Max/Song::Rating_Step;
static const int constBorder=2;

RatingPainter::RatingPainter(int s)
    : starSz(s)
    , pixmapSize((starSz*constNumStars)+(constBorder*(constNumStars-1)), starSz)
    , col(QApplication::palette().text().color())
{
}

void RatingPainter::paint(QPainter *p, const QRect &r, int rating)
{
    if (rating<0 || rating>Song::Rating_Max) {
        return;
    }

    double pixelRatio=p && p->device() ? p->device()->DEVICE_PIXEL_RATIO() : 1.0;
    if (!isNull() && !Utils::equal(pixelRatio, pixmaps[0].DEVICE_PIXEL_RATIO())) {
        pixmaps[0]=QPixmap();
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
            if (pixelRatio>1.25) {
                bytes.replace("stroke-width=\"3\"", "stroke-width=\"6\"");
            }
        }
        renderer.load(bytes);

        int pixSize=starSz*pixelRatio;

        for (int p=0; p<2; ++p) {
            pixmaps[p]=QPixmap(pixSize, pixSize);
            pixmaps[p].fill(Qt::transparent);
            QPainter painter(&(pixmaps[p]));
            renderer.render(&painter, 1==p ? "on" : "off", QRectF(0, 0, pixSize, pixSize));
        }
        pixmaps[2]=QPixmap(pixSize, pixSize);
        pixmaps[2].fill(Qt::transparent);
        QPainter painter(&(pixmaps[2]));
        int halfSz=(pixSize/2.0)+0.5;
        painter.drawPixmap(0, 0, pixmaps[1], 0, 0, halfSz, pixSize);
        painter.drawPixmap(halfSz, 0, pixmaps[0], halfSz, 0, pixSize-halfSz, pixSize);
        painter.end();
        for (int p=0; p<3; ++p) {
            pixmaps[p].setDevicePixelRatio(pixelRatio);
        }
    }

    int fullStars=rating/Song::Rating_Step;
    bool half=allowHalfStars && rating%Song::Rating_Step;
    QSize layoutSize = pixmaps[0].size() / pixmaps[0].DEVICE_PIXEL_RATIO();
    QRect pr(r.x(), r.y()+(r.height()-layoutSize.width())/2, layoutSize.width(), layoutSize.height());

    for (int i=0; i<constNumStars; ++i) {
        p->drawPixmap(pr, pixmaps[half && i==fullStars ? 2 : i<fullStars ? 1 : 0]);
        pr.setX(pr.x()+layoutSize.width()+constBorder);
    }
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
    , showZeroForNull(false)
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
    rp.paint(&p, rect(), -1==hoverVal ? (showZeroForNull && val==Song::Rating_Null ? 0 : val) : hoverVal);
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
    const int width = rp.size().width();
    const int x = r.x() + (r.width() - width) / 2;
    return QRect(x, r.y(), width, r.height());
}

int RatingWidget::valueForPos(const QPoint &pos) const
{
    const QRect contents = contentsRect();
    const double raw = double(pos.x() - contents.left()) / contents.width();
    if (!allowHalfStars) {
        int v=(raw*Song::Rating_Max);
        if (v%Song::Rating_Step) {
            v++;
        }
        return v;
    }
    return (raw*Song::Rating_Max)+0.5;
}

#include "moc_ratingwidget.cpp"
