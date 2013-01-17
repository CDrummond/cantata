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

#include "messagebox.h"
#include "icon.h"
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
                               const GuiItem &yesText, const GuiItem &noText, bool showCancel, bool isWarning)
{
    if (yesText.text.isEmpty() && noText.text.isEmpty()) {
        return map(isWarning
                ? QMessageBox::warning(parent, title.isEmpty() ? i18n("Warning") : title, message,
                                       QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton))
                : QMessageBox::question(parent, title.isEmpty() ? i18n("Question") : title, message,
                                        QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton))
               );
    } else {
        QMessageBox box(isWarning ? QMessageBox::Warning : QMessageBox::Question, title.isEmpty() ? (isWarning ? i18n("Warning") : i18n("Question")) : title,
                        message, QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton), parent);

        box.setDefaultButton(QMessageBox::Yes);
        if (!yesText.text.isEmpty()) {
            QAbstractButton *btn=box.button(QMessageBox::Yes);
            btn->setText(yesText.text);
            if (!yesText.icon.isEmpty()) {
                btn->setIcon(Icon(yesText.icon));
            }
        }
        if (!noText.text.isEmpty()) {
            QAbstractButton *btn=box.button(QMessageBox::No);
            btn->setText(noText.text);
            if (!noText.icon.isEmpty()) {
                btn->setIcon(Icon(noText.icon));
            }
        }
        return -1==box.exec() ? Cancel : map(box.standardButton(box.clickedButton()));
    }
}

void MessageBox::errorList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title)
{
    QMessageBox box(QMessageBox::Critical, title.isEmpty() ? i18n("Error") : title, message, QMessageBox::Ok, parent);
    box.setDetailedText(strlist.join("\n"));
    box.exec();
}
