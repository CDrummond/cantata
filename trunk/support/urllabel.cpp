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

#include "urllabel.h"
#include <QVariant>
#include <QMouseEvent>
#include <QApplication>
#include <QCursor>

UrlLabel::UrlLabel(QWidget *p)
    : QLabel(p)
    , pressed(false)
{
    setCursor(QCursor(Qt::PointingHandCursor));
    setContextMenuPolicy(Qt::NoContextMenu);
}

void UrlLabel::setText(const QString &t)
{
    QLabel::setText("<a href=\".\">"+t+"</a>");
}

void UrlLabel::setProperty(const char *name, const QVariant &value)
{
    if (name && !strcmp(name, "text") && QVariant::String==value.type()) {
        setText(value.toString());
    }
}

void UrlLabel::mousePressEvent(QMouseEvent *ev)
{
    if (Qt::LeftButton==ev->buttons()) {
        pressed=true;
    }
}

void UrlLabel::mouseReleaseEvent(QMouseEvent *)
{
    if (pressed) {
        pressed=false;
        if (this==QApplication::widgetAt(QCursor::pos())) {
            emit leftClickedUrl();
        }
    }
}
