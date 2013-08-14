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
#include "dialog.h"
#include "localize.h"
#include "config.h"
#include "gtkstyle.h"
#include "acceleratormanager.h"
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#ifndef ENABLE_KDE_SUPPORT
#include <QAbstractButton>

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
    QMessageBox box(isWarning ? QMessageBox::Warning : QMessageBox::Question, title.isEmpty() ? (isWarning ? i18n("Warning") : i18n("Question")) : title,
                    message, QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton), parent);

    box.setDefaultButton(isWarning && !showCancel ? QMessageBox::No : QMessageBox::Yes);
    if (!yesText.text.isEmpty()) {
        QAbstractButton *btn=box.button(QMessageBox::Yes);
        btn->setText(yesText.text);
        if (!yesText.icon.isEmpty() && !GtkStyle::isActive()) {
            btn->setIcon(Icon(yesText.icon));
        }
    }
    if (!noText.text.isEmpty()) {
        QAbstractButton *btn=box.button(QMessageBox::No);
        btn->setText(noText.text);
        if (!noText.icon.isEmpty() && !GtkStyle::isActive()) {
            btn->setIcon(Icon(noText.icon));
        }
    }
    AcceleratorManager::manage(&box);
    return -1==box.exec() ? Cancel : map(box.standardButton(box.clickedButton()));
}
#endif

void MessageBox::errorListEx(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title)
{
    Dialog *dlg=new Dialog(parent);
    dlg->setCaption(title.isEmpty() ? i18n("Error") : title);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setButtons(Dialog::Ok);
    QWidget *wid=new QWidget(dlg);
    QGridLayout *lay=new QGridLayout(wid);
    QLabel *iconLabel=new QLabel(wid);
    int iconSize=Icon::dlgIconSize();
    iconLabel->setMinimumSize(iconSize, iconSize);
    iconLabel->setMaximumSize(iconSize, iconSize);
    iconLabel->setPixmap(Icon("dialog-error").pixmap(iconSize, iconSize));
    lay->addWidget(iconLabel, 0, 0, 1, 1);
    lay->addWidget(new QLabel(message, wid), 0, 1, 1, 1);
    QListWidget *list=new QListWidget(wid);
    lay->addWidget(list, 1, 0, 1, 2);
    list->insertItems(0, strlist);
    dlg->setMainWidget(wid);
    dlg->exec();
}
