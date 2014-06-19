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

#include "coverwidget.h"
#include "gui/currentcover.h"
#include "gui/covers.h"
#include "mpd/song.h"
#include "context/view.h"
#include "support/localize.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPen>

static const int constBorder=1;

CoverWidget::CoverWidget(QWidget *parent)
    : QLabel(parent)
    , pix(0)
{
    setStyleSheet(QString("QLabel {border: %1px solid transparent} QToolTip {background-color:#111111; color: #DDDDDD}").arg(constBorder));
}

CoverWidget::~CoverWidget()
{
}

void CoverWidget::setSize(int min)
{
    setFixedSize(min, min);
}

void CoverWidget::setEnabled(bool e)
{
    if (e) {
        connect(CurrentCover::self(), SIGNAL(coverImage(QImage)), this, SLOT(coverImage(QImage)));
        coverImage(QImage());
    } else {
        if (pix) {
            delete pix;
            pix=0;
        }
        disconnect(CurrentCover::self(), SIGNAL(coverImage(QImage)), this, SLOT(coverImage(QImage)));
    }
    setVisible(e);
    QLabel::setEnabled(e);
}

bool CoverWidget::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::ToolTip: {
        const Song &current=CurrentCover::self()->song();
        if (current.isEmpty() || (current.isStream() && !current.isCantataStream() && !current.isCdda())) {
            setToolTip(QString());
            break;
        }
        QString toolTip=QLatin1String("<table>");
        const QImage &img=CurrentCover::self()->image();

        if (Song::useComposer() && !current.composer().isEmpty() && current.composer()!=current.albumArtist()) {
            toolTip+=i18n("<tr><td align=\"right\"><b>Composer:</b></td><td>%1</td></tr>", current.composer());
        }
        toolTip+=i18n("<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                      "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                      "<tr><td align=\"right\"><b>Year:</b></td><td>%3</td></tr>", current.albumArtist(), current.album, QString::number(current.year));
        toolTip+="</table>";
        if (!img.isNull()) {
            if (img.size().width()>Covers::constMaxSize.width() || img.size().height()>Covers::constMaxSize.height() ||
                CurrentCover::self()->fileName().isEmpty() || !QFile::exists(CurrentCover::self()->fileName())) {
                toolTip+=QString("<br/>%1").arg(View::encode(img));
            } else {
                toolTip+=QString("<br/><img src=\"%1\"/>").arg(CurrentCover::self()->fileName());
            }
        }
        setToolTip(toolTip);
        break;
    }
    default:
        break;
    }
    return QLabel::event(event);
}

void CoverWidget::paintEvent(QPaintEvent *)
{
    if (!pix) {
        return;
    }
    QPainter p(this);
    p.drawPixmap((width()-pix->width())/2, (height()-pix->height())/2, *pix);
}

static QPainterPath buildPath(const QRectF &r, double radius)
{
    QPainterPath path;
    double diameter(radius*2);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
    return path;
}

void CoverWidget::coverImage(const QImage &)
{
    QImage img=CurrentCover::self()->image();
    if (img.isNull()) {
        return;
    }
    int size=height()-(2*constBorder);
    img=img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (pix && pix->size()!=img.size()) {
        delete pix;
        pix=0;
    }
    if (!pix) {
        pix=new QPixmap(img.size());
    }
    pix->fill(Qt::transparent);
    QPainter painter(pix);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path=buildPath(QRectF(0.5, 0.5, img.width()-1, img.height()-1), img.width()>128 ? 6.0 : 4.0);
    painter.fillPath(path, img);

//    QPainterPath glassPath;
//    glassPath.moveTo(pix->rect().topLeft());
//    glassPath.lineTo(pix->rect().topRight());
//    glassPath.quadTo(pix->rect().center()/2, pix->rect().bottomLeft());
//    painter.setClipPath(path);
//    painter.setPen(Qt::NoPen);
//    painter.setBrush(QColor(255, 255, 255, 64));
//    painter.drawPath(glassPath);
//    painter.setClipping(false);
    QColor col=palette().foreground().color();

    col.setAlpha(128);
    if (col.red()>=196 && col.blue()>=196 && col.green()>=196) {
        QLinearGradient grad(0, 0, 0, height());
        grad.setColorAt(0, QColor(0, 0, 0, 128));
        grad.setColorAt(1, col);
        painter.strokePath(path, QPen(grad, 1));
    } else {
        painter.strokePath(path, QPen(col, 1));
    }
    repaint();
}
