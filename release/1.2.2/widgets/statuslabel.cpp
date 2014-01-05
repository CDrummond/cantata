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
#ifdef ENABLE_KDE_SUPPORT
#include <kcolorscheme.h>
#endif

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

#ifdef ENABLE_KDE_SUPPORT
static void getColorsFromColorScheme(KColorScheme::BackgroundRole bgRole, QColor* bg, QColor* fg)
{
    KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    *bg = scheme.background(bgRole).color();
    *fg = scheme.foreground().color();
}
#endif

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
        Icon icn;

        switch (type) {
        case Error: {
            QColor bg, border, fg;
            icn=Icon("dialog-error");
            #ifdef ENABLE_KDE_SUPPORT
            getColorsFromColorScheme(KColorScheme::NegativeBackground, &bg, &fg);
            border = KColorScheme::shade(bg, KColorScheme::DarkShade);
            #else
            bg=QColor(0xeb, 0xbb, 0xbb);
            fg=Qt::black;
            border = Qt::red;
            #endif
            bg.setAlphaF(0.5);
            setStyleSheet(QString(".QFrame {"
                                      "background-color: %1;"
                                      "border-radius: 3px;"
                                      "border: 1px solid %2;"
                                      "margin: %3px;"
                                      "}"
                                      ".QLabel { color: %4; }")
                            .arg(toString(bg))
                            .arg(toString(border))
                                // DefaultFrameWidth returns the size of the external margin + border width. We know our border is 1px,
                                // so we subtract this from the frame normal QStyle FrameWidth to get our margin
                            .arg(style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this) -1)
                            .arg(toString(fg)));
            break;
        }
        case Locked:
            icn=Icon("object-locked");
            if (icn.isNull()) {
                icn=Icon("locked");
            }
            setStyleSheet(QString(".QFrame {"
                                      "border-radius: 3px;"
                                      "border: 1px solid %1;"
                                      "margin: %2px;"
                                      "}")
                            .arg(toString(palette().highlight().color()))
                                // DefaultFrameWidth returns the size of the external margin + border width. We know our border is 1px,
                                // so we subtract this from the frame normal QStyle FrameWidth to get our margin
                            .arg(style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this) -1));
        default:
            break;
        }

        icon->setPixmap(icn.pixmap(iconSize, iconSize));
        icon->setVisible(true);
    }
}
