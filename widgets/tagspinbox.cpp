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

#include "tagspinbox.h"
#include <QPainter>
#include <QLineEdit>

static QString variousText;

QString TagSpinBox::variousStr() { return variousText; }

TagSpinBox::TagSpinBox(QWidget *parent)
    : EmptySpinBox(parent)
    , isVarious(false)
{
    if (variousText.isEmpty()) {
        variousText=tr("(Various)");
    }
}

QSize TagSpinBox::sizeHint() const
{
    TagSpinBox *that=const_cast<TagSpinBox *>(this);
    that->setSpecialValueText(variousText);
    QSize sz=EmptySpinBox::sizeHint();
    that->setSpecialValueText(QString());
    return sz;
}

void TagSpinBox::setVarious(bool v)
{
    if (v==isVarious) {
        return;
    }
    isVarious=v;
    lineEdit()->setPlaceholderText(isVarious ? variousText : QString());
}

#include "moc_tagspinbox.cpp"
