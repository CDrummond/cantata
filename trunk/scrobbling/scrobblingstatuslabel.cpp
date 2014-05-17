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

#include "scrobblingstatuslabel.h"
#include "support/localize.h"
#include "scrobbler.h"
#include "widgets/icons.h"
#include "widgets/sizewidget.h"
#include "widgets/toolbutton.h"
#include <QPalette>
#include <QStyle>
#include <QApplication>
#include <QCursor>

ScrobblingStatusLabel::ScrobblingStatusLabel(QWidget *p)
    : QLabel(p)
    , pressed(false)
{
    setEnabled(true);
    ToolButton tb;
    tb.move(65535, 65535);
    tb.setToolButtonStyle(Qt::ToolButtonIconOnly);
    tb.setIcon(Icons::self()->albumIcon);
    tb.ensurePolished();
    tb.setAttribute(Qt::WA_DontShowOnScreen);
    tb.setVisible(true);
    imageSize=tb.height()*0.75;
    setFixedSize(tb.height()+4, tb.height());
    connect(Scrobbler::self(), SIGNAL(authenticated(bool)), SLOT(setVisible(bool)));
    connect(Scrobbler::self(), SIGNAL(enabled(bool)), SLOT(setStatus(bool)));
    setVisible(Scrobbler::self()->isAuthenticated());
    setStatus(Scrobbler::self()->isScrobblingEnabled());
}

void ScrobblingStatusLabel::setStatus(bool on)
{
    setToolTip(on ? i18n("Scrobbling Enabled") : i18n("Scrobbling Disabled"));
    setPixmap(Icons::self()->lastFmIcon.pixmap(imageSize, imageSize, on ? QIcon::Normal : QIcon::Disabled));
}

void ScrobblingStatusLabel::mousePressEvent(QMouseEvent *ev)
{
    QLabel::mousePressEvent(ev);
    pressed=true;
}

void ScrobblingStatusLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    QLabel::mouseReleaseEvent(ev);
    if (pressed) {
        pressed=false;
        if (this==QApplication::widgetAt(QCursor::pos())) {
            emit clicked();
        }
    }
}
