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

#include "toolbutton.h"
#include "support/icon.h"
#include "support/gtkstyle.h"
#include "support/touchproxystyle.h"
#include "config.h"
#include "support/utils.h"
#include <QMenu>
#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#ifdef Q_OS_MAC
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
#endif

ToolButton::ToolButton(QWidget *parent)
    : QToolButton(parent)
    #ifdef USE_SYSTEM_MENU_ICON
    , hideMenuIndicator(GtkStyle::isActive())
    #else
    , hideMenuIndicator(true)
    #endif
{
    Icon::init(this);
    #ifdef Q_OS_MAC
    setStyleSheet("QToolButton {border: 0}");
    #endif
    setFocusPolicy(Qt::NoFocus);
}

void ToolButton::paintEvent(QPaintEvent *e)
{
    #ifdef Q_OS_MAC
    if (isDown() || isChecked()) {
        QPainter p(this);
        QColor col(palette().color(QPalette::WindowText));
        QRect r(rect());
        QPainterPath path=buildPath(QRectF(r.x()+1.5, r.y()+0.5, r.width()-2, r.height()-1), 2.5);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(col);
        p.drawPath(path);
        col.setAlphaF(0.1);
        p.fillPath(path, col);
    }
    #endif
    #if QT_VERSION > 0x050000
    // Hack to work-around Qt5 sometimes leaving toolbutton in 'raised' state.
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    if (hideMenuIndicator) {
        opt.features=QStyleOptionToolButton::None;
    }
    if (opt.state&QStyle::State_MouseOver && this!=QApplication::widgetAt(QCursor::pos())) {
        opt.state&=~QStyle::State_MouseOver;
    }
    p.drawComplexControl(QStyle::CC_ToolButton, opt);
    #else
    if (menu() && hideMenuIndicator) {
        QStylePainter p(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.features=QStyleOptionToolButton::None;
        p.drawComplexControl(QStyle::CC_ToolButton, opt);
    } else {
        QToolButton::paintEvent(e);
    }
    #endif
}

QSize ToolButton::sizeHint() const
{
    if (!hideMenuIndicator) {
        QSize sz=QToolButton::sizeHint();
        return Utils::touchFriendly() ? QSize(sz.width()*TouchProxyStyle::constScaleFactor, sz.height()) : sz;
    }

    if (sh.isValid()) {
        return sh;
    }

    ensurePolished();

    if (menu()) {
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.features=QStyleOptionToolButton::None;
        sh = style()->sizeFromContents(QStyle::CT_ToolButton, &opt, opt.iconSize, this).expandedTo(QApplication::globalStrut());
    } else {
        sh=QToolButton::sizeHint();
    }
    sh=QSize(qMax(sh.width(), sh.height())*(Utils::touchFriendly() ? TouchProxyStyle::constScaleFactor : 1.0), qMax(sh.width(), sh.height()));
    return sh;
}

void ToolButton::setMenu(QMenu *m)
{
    QToolButton::setMenu(m);
    sh=QSize();
    setPopupMode(InstantPopup);
}
