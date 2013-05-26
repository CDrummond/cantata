/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "statuslabel.h"
#include "icon.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QApplication>
#include <QStyle>

static int iconSize=-1;

StatusLabel::StatusLabel(QWidget *p)
    : QFrame(p)
    , type(None)
{
    setFrameShape(QFrame::StyledPanel);
    QHBoxLayout *layout = new QHBoxLayout(this);
    icon = new QLabel(this);
    text = new SqueezedTextLabel(this);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(text->sizePolicy().hasHeightForWidth());
    text->setSizePolicy(sizePolicy);
    layout->addWidget(icon);
    layout->addWidget(text);
    
    if (-1==iconSize) {
        iconSize=Icon::stdSize(QApplication::fontMetrics().height());
    }
    icon->setVisible(false);
    layout->setMargin(layout->margin()/2);
}

static QString toString(const QColor &col)
{
    return QString("rgba(%1, %2, %3, %4)").arg(col.red()).arg(col.green()).arg(col.blue()).arg(col.alpha());
}

void StatusLabel::setType(Type t)
{
    if (t==type) {
        return;
    }
    type=t;
    if (None==type) {
        icon->setVisible(false);
        setStyleSheet(QString());
    } else {
        static const double constAlpha=0.25;
        
        Icon icn;
        QColor bg0, bg1, bg2, border;

        switch (type) {
        case Error:
            icn=Icon("dialog-error");
            bg1=QColor(0xeb, 0xbb, 0xbb);
            border = bg1.darker(150);
            break;
        case Locked:
            icn=Icon("object-locked");
            if (icn.isNull()) {
                icn=Icon("locked");
            }
            bg1 = palette().highlight().color();
            border = bg1.darker(150);
        default:
            break;
        }

        bg0 = bg1.lighter(110);
        bg2 = bg1.darker(110);
        bg0.setAlphaF(constAlpha);
        bg1.setAlphaF(constAlpha);
        bg2.setAlphaF(constAlpha);
        border.setAlphaF(constAlpha*2.5);
        
        icon->setPixmap(icn.pixmap(iconSize, iconSize));
        icon->setVisible(true);
        setStyleSheet(QString(".QFrame {"
                                  "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 %1, stop: 0.1 %2, stop: 1.0 %3);"
                                  "border-radius: 3px;"
                                  "border: 1px solid %4;"
                                  "margin: %5px;"
                                  "}")
                        .arg(toString(bg0))
                        .arg(toString(bg1))
                        .arg(toString(bg2))
                        .arg(toString(border))
                            // DefaultFrameWidth returns the size of the external margin + border width. We know our border is 1px,
                            // so we subtract this from the frame normal QStyle FrameWidth to get our margin
                        .arg(style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this) -1));
    }
}
