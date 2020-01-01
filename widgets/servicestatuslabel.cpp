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

#include "servicestatuslabel.h"
#include <QPalette>
#include <QStyle>
#include <QApplication>
#include <QCursor>

ServiceStatusLabel::ServiceStatusLabel(QWidget *p)
    : QLabel(p)
    , pressed(false)
{
    QFont f(font());
    f.setBold(true);
    setFont(f);
}

void ServiceStatusLabel::setText(const QString &txt, const QString &name)
{
    QLabel::setText(txt);
    onTooltip=tr("Logged into %1").arg(name);
    offTooltip=tr("<b>NOT</b> logged into %1").arg(name);
}

void ServiceStatusLabel::setStatus(bool on)
{
    setVisible(true);
    setToolTip(on ? onTooltip : offTooltip);
    QString col=on
            ? palette().highlight().color().name()
            : palette().color(QPalette::Disabled, QPalette::WindowText).name();

    int margin=style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this);
    if (margin<2) {
        margin=2;
    }
    setStyleSheet(QString("QLabel { color : %1; border-radius: %4px; border: 2px solid %2; margin: %3px}")
                  .arg(col).arg(col).arg(margin).arg(margin*2));
}

void ServiceStatusLabel::mousePressEvent(QMouseEvent *ev)
{
    QLabel::mousePressEvent(ev);
    pressed=true;
}

void ServiceStatusLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    QLabel::mouseReleaseEvent(ev);
    if (pressed) {
        pressed=false;
        if (this==QApplication::widgetAt(QCursor::pos())) {
            emit clicked();
        }
    }
}


#include "moc_servicestatuslabel.cpp"
