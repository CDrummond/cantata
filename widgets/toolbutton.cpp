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

#include "toolbutton.h"
#include "support/icon.h"
#include "support/gtkstyle.h"
#include "config.h"
#include "support/utils.h"
#include <QMenu>
#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QApplication>
#include <QPainter>

ToolButton::ToolButton(QWidget *parent)
    : QToolButton(parent)
{
    Icon::init(this);
    #ifdef Q_OS_MAC
    setStyleSheet("QToolButton {border: 0}");
    allowMouseOver=parent && parent->objectName()!=QLatin1String("toolbar");
    #endif
    setFocusPolicy(Qt::NoFocus);
}

void ToolButton::paintEvent(QPaintEvent *e)
{
    #ifdef Q_OS_MAC
    bool down=isDown() || isChecked();
    bool mo=false;

    if (allowMouseOver && !down && isEnabled()) {
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        mo=opt.state&QStyle::State_MouseOver && this==QApplication::widgetAt(QCursor::pos());
    }
    if (down || mo) {
        QPainter p(this);
        QColor col(palette().color(QPalette::WindowText));
        QRect r(rect());
        QPainterPath path=Utils::buildPath(QRectF(r.x()+1.5, r.y()+1.5, r.width()-3, r.height()-3), 2.5);
        p.setRenderHint(QPainter::Antialiasing, true);
        col.setAlphaF(0.4);
        p.setPen(col);
        p.drawPath(path);
        if (down) {
            col.setAlphaF(0.1);
            p.fillPath(path, col);
        }
    }
    #endif
    Q_UNUSED(e)
    // Hack to work-around Qt5 sometimes leaving toolbutton in 'raised' state.
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    opt.features=QStyleOptionToolButton::None;
    if (opt.state&QStyle::State_MouseOver && this!=QApplication::widgetAt(QCursor::pos())) {
        opt.state&=~QStyle::State_MouseOver;
    }
    p.drawComplexControl(QStyle::CC_ToolButton, opt);
}

QSize ToolButton::sizeHint() const
{
    if (!sh.isValid()) {
        ensurePolished();
        sh = QToolButton::sizeHint();

        if (sh.width()>sh.height()) {
            sh.setWidth(sh.height());
        }

        sh=QSize(qMax(sh.width(), sh.height()), qMax(sh.width(), sh.height()));
        #ifdef Q_OS_MAC
        sh=QSize(qMax(sh.width(), 22), qMax(sh.height(), 20));
        #endif
    }
    return sh;
}

void ToolButton::setMenu(QMenu *m)
{
    QToolButton::setMenu(m);
    sh=QSize();
    setPopupMode(InstantPopup);
}
