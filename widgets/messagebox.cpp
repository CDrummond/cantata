/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "messagebox.h"
#include <QtGui/QAbstractButton>

MessageBox::ButtonCode map(QMessageBox::StandardButton c)
{
    switch (c) {
    case QMessageBox::Yes:    return MessageBox::Yes;
    case QMessageBox::No:     return MessageBox::No;
    default:
    case QMessageBox::Cancel: return MessageBox::Cancel;
    }
}

MessageBox::ButtonCode MessageBox::questionYesNoCancel(QWidget *parent, const QString &message, const QString &title,
                               const QString &yesText, const QString &noText, bool showCancel, bool isWarning) {
    if (yesText.isEmpty() && noText.isEmpty()) {
        return map(isWarning
                ? QMessageBox::warning(parent, message, title.isEmpty() ? QObject::tr("Warning") : title,
                                       QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton))
                : QMessageBox::question(parent, message, title.isEmpty() ? QObject::tr("Question") : title,
                                        QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton))
               );
    } else {
        QMessageBox box(isWarning ? QMessageBox::Warning : QMessageBox::Question, title.isEmpty() ? (isWarning ? QObject::tr("Warning") : QObject::tr("Question")) : title,
                        message, QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton), parent);

        box.setDefaultButton(QMessageBox::Yes);
        if (!yesText.isEmpty()) {
            box.button(QMessageBox::Yes)->setText(yesText);
        }
        if (!noText.isEmpty()) {
            box.button(QMessageBox::No)->setText(noText);
        }
        return -1==box.exec() ? Cancel : map(box.standardButton(box.clickedButton()));
    }
}
