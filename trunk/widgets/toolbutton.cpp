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
#include "icon.h"
#include "gtkstyle.h"
#include "config.h"
#include "utils.h"
#include <QMenu>
#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QApplication>

const double ToolButton::constTouchScaleFactor=1.334;

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
    if (sh.isValid()) {
        return sh;
    }

    ensurePolished();

    QSize plainSize;
    QSize menuSize;
    if (menu()) {
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.features=QStyleOptionToolButton::None;
        plainSize = style()->sizeFromContents(QStyle::CT_ToolButton, &opt, opt.iconSize, this).expandedTo(QApplication::globalStrut());
        menuSize = QToolButton::sizeHint();
    } else {
        plainSize = menuSize = QToolButton::sizeHint();
    }
    int sz=qMax(plainSize.width(), plainSize.height());
    sh=QSize(Utils::touchFriendly() ? sz*constTouchScaleFactor : (hideMenuIndicator ? sz : menuSize.width()), sz);
    return sh;
}

void ToolButton::setMenu(QMenu *m)
{
    QToolButton::setMenu(m);
    sh=QSize();
    setPopupMode(InstantPopup);
}
