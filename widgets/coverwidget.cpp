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
#include "support/gtkstyle.h"
#include "support/utils.h"
#ifdef ENABLE_ONLINE_SERVICES
#include "online/onlineservice.h"
#endif
#include <QLinearGradient>
#include <QPen>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>
#include <QBoxLayout>
#include <QSpacerItem>

static const int constBorder=1;

class CoverLabel : public QLabel
{
public:
    CoverLabel(QWidget *p)
        : QLabel(p)
        , pressed(false)
        , pix(0)
    {
    }

    bool event(QEvent *event)
    {
        switch(event->type()) {
        case QEvent::ToolTip: {
            const Song &current=CurrentCover::self()->song();
            if (current.isEmpty() || (current.isStream() && !current.isCantataStream() && !current.isCdda())
                #ifdef ENABLE_ONLINE_SERVICES
                || OnlineService::showLogoAsCover(current)
                #endif
                ) {
                setToolTip(QString());
                break;
            }
            QString toolTip=QLatin1String("<table>");
            const QImage &img=CurrentCover::self()->image();

            if (!current.composer().isEmpty()) {
                toolTip+=i18n("<tr><td align=\"right\"><b>Composer:</b></td><td>%1</td></tr>", current.composer());
            }
            if (!current.performer().isEmpty() && current.performer()!=current.albumArtist()) {
                toolTip+=i18n("<tr><td align=\"right\"><b>Performer:</b></td><td>%1</td></tr>", current.performer());
            }
            toolTip+=i18n("<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                          "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                          "<tr><td align=\"right\"><b>Year:</b></td><td>%3</td></tr>", current.albumArtist(), current.album, QString::number(current.year));
            toolTip+="</table>";
            if (!img.isNull()) {
                if (img.size().width()>Covers::constMaxSize.width() || img.size().height()>Covers::constMaxSize.height()) {
                    toolTip+=QString("<br/>%1").arg(View::encode(img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                } else if (CurrentCover::self()->fileName().isEmpty() || !QFile::exists(CurrentCover::self()->fileName())) {
                    toolTip+=QString("<br/>%1").arg(View::encode(img));
                } else {
                    toolTip+=QString("<br/><img src=\"%1\"/>").arg(CurrentCover::self()->fileName());
                }
            }
            setToolTip(toolTip);
            break;
        }
        case QEvent::MouseButtonPress:
            if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button() && Qt::NoModifier==static_cast<QMouseEvent *>(event)->modifiers()) {
                pressed=true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (pressed && Qt::LeftButton==static_cast<QMouseEvent *>(event)->button() && !QApplication::overrideCursor()) {
                static_cast<CoverWidget*>(parentWidget())->emitClicked();
            }
            pressed=false;
            break;
        default:
            break;
        }
        return QLabel::event(event);
    }

    void paintEvent(QPaintEvent *)
    {
        if (!pix) {
            return;
        }
        QPainter p(this);
        QRect r((width()-pix->width())/2, (height()-pix->height())/2, pix->width(), pix->height());
        p.drawPixmap(r.x(), r.y(), *pix);
        if (underMouse()) {
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(palette().color(QPalette::Highlight), 2));
            p.drawPath(Utils::buildPath(QRectF(r.x()+0.5, r.y()+0.5, r.width()-1, r.height()-1), pix->width()>128 ? 6 : 4));
        }
    }

    void updatePix()
    {
        QImage img=CurrentCover::self()->image();
        if (img.isNull()) {
            return;
        }
        int size=height();
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
        painter.fillPath(Utils::buildPath(QRectF(0, 0, img.width(), img.height()), img.width()>128 ? 6.5 : 4.5), img);
        repaint();
    }

    void deletePix()
    {
        if (pix) {
            delete pix;
            pix=0;
        }
    }

private:
    bool pressed;
    QPixmap *pix;
};

CoverWidget::CoverWidget(QWidget *parent)
    : QWidget(parent)
{
    QBoxLayout *l=new QBoxLayout(QBoxLayout::LeftToRight, this);
    l->setMargin(0);
    l->setSpacing(0);
    l->addItem(new QSpacerItem(Utils::layoutSpacing(this), 4, QSizePolicy::Fixed, QSizePolicy::Fixed));
    label=new CoverLabel(this);
    l->addWidget(label);
    label->setStyleSheet(QString("QLabel {border: %1px solid transparent} QToolTip {background-color:#111111; color: #DDDDDD}").arg(constBorder));
    label->setAttribute(Qt::WA_Hover, true);
}

CoverWidget::~CoverWidget()
{
}

void CoverWidget::setSize(int min)
{
    #ifdef Q_OS_MAC
    min*=0.9;
    #endif
    label->setFixedSize(min, min);
}

void CoverWidget::setEnabled(bool e)
{
    if (e) {
        connect(CurrentCover::self(), SIGNAL(coverImage(QImage)), this, SLOT(coverImage(QImage)));
        coverImage(QImage());
    } else {
        label->deletePix();
        disconnect(CurrentCover::self(), SIGNAL(coverImage(QImage)), this, SLOT(coverImage(QImage)));
    }
    setVisible(e);
    label->setEnabled(e);
}

void CoverWidget::coverImage(const QImage &)
{
    label->updatePix();
}
