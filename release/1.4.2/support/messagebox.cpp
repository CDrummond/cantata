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
        btn->setIcon(yesText.icon.isEmpty() || GtkStyle::isActive() ? Icon() : Icon(yesText.icon));
    }
    if (!noText.text.isEmpty()) {
        QAbstractButton *btn=box.button(QMessageBox::No);
        btn->setText(noText.text);
        btn->setIcon(noText.icon.isEmpty() || GtkStyle::isActive() ? Icon() : Icon(noText.icon));
    }
    AcceleratorManager::manage(&box);
    return -1==box.exec() ? Cancel : map(box.standardButton(box.clickedButton()));
}
#endif

#ifdef ENABLE_KDE_SUPPORT
void MessageBox::errorListEx(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title)
#else
namespace MessageBox
{
class YesNoListDialog : public Dialog
{
public:
    YesNoListDialog(QWidget *p) : Dialog(p) { }
    void slotButtonClicked(int b) {
        switch(b) {
        case Dialog::Ok:
        case Dialog::Yes:
            accept();
            break;
        case Dialog::No:
            reject();
            break;
        }
    }
};
}

MessageBox::ButtonCode MessageBox::msgListEx(QWidget *parent, Type type, const QString &message, const QStringList &strlist, const QString &title)
#endif
{
    #ifdef ENABLE_KDE_SUPPORT
    Dialog *dlg=new Dialog(parent);
    #else
    MessageBox::YesNoListDialog *dlg=new MessageBox::YesNoListDialog(parent);
    #endif
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QWidget *wid=new QWidget(dlg);
    QGridLayout *lay=new QGridLayout(wid);
    QLabel *iconLabel=new QLabel(wid);
    int iconSize=Icon::dlgIconSize();
    iconLabel->setMinimumSize(iconSize, iconSize);
    iconLabel->setMaximumSize(iconSize, iconSize);
    #ifdef ENABLE_KDE_SUPPORT
    dlg->setCaption(title.isEmpty() ? i18n("Error") : title);
    dlg->setButtons(Dialog::Ok);
    iconLabel->setPixmap(Icon("dialog-error").pixmap(iconSize, iconSize));
    #else
    switch(type) {
    case Error:
        dlg->setCaption(title.isEmpty() ? i18n("Error") : title);
        dlg->setButtons(Dialog::Ok);
        iconLabel->setPixmap(Icon("dialog-error").pixmap(iconSize, iconSize));
        break;
    case Question:
        dlg->setCaption(title.isEmpty() ? i18n("Question") : title);
        dlg->setButtons(Dialog::Yes|Dialog::No);
        iconLabel->setPixmap(Icon("dialog-information").pixmap(iconSize, iconSize));
        break;
    case Warning:
        dlg->setCaption(title.isEmpty() ? i18n("Warning") : title);
        dlg->setButtons(Dialog::Yes|Dialog::No);
        iconLabel->setPixmap(Icon("dialog-warning").pixmap(iconSize, iconSize));
        break;
    }
    #endif
    lay->addWidget(iconLabel, 0, 0, 1, 1);
    lay->addWidget(new QLabel(message, wid), 0, 1, 1, 1);
    QListWidget *list=new QListWidget(wid);
    lay->addWidget(list, 1, 0, 1, 2);
    list->insertItems(0, strlist);
    dlg->setMainWidget(wid);
    #ifdef ENABLE_KDE_SUPPORT
    dlg->exec();
    #else
    return QDialog::Accepted==dlg->exec() ? Yes : No;
    #endif
}
