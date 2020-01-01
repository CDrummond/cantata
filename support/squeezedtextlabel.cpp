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

#include "squeezedtextlabel.h"

SqueezedTextLabel::SqueezedTextLabel(QWidget *p)
    : QLabel(p)
{
    setTextElideMode(isRightToLeft() ? Qt::ElideLeft : Qt::ElideRight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void SqueezedTextLabel::setTextElideMode(Qt::TextElideMode mode)
{
    elideMode=mode;
    setAlignment((Qt::ElideLeft==elideMode ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
}

void SqueezedTextLabel::elideText()
{
    QFontMetrics fm(fontMetrics());
    int labelWidth = size().width();
    int lineWidth = fm.width(originalText);

    if (lineWidth > labelWidth) {
        QLabel::setText(fm.elidedText(originalText, elideMode, labelWidth));
        setToolTip(originalText);
    } else {
        QLabel::setText(originalText);
        setToolTip(QString());
    }
}
