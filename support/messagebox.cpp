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

#include "messagebox.h"
#include "icon.h"
#include "dialog.h"
#include "config.h"
#include "gtkstyle.h"
#include "acceleratormanager.h"
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QAbstractButton>
#include <QStyle>

MessageBox::ButtonCode map(QMessageBox::StandardButton c)
{
    switch (c) {
    case QMessageBox::Yes:    return MessageBox::Yes;
    case QMessageBox::No:     return MessageBox::No;
    default:
    case QMessageBox::Cancel: return MessageBox::Cancel;
    }
}

#ifdef Q_OS_MAC
static void splitMessage(const QString &orig, QString &msg, QString &sub)
{
    static QStringList constSeps=QStringList() << QLatin1String("\n\n") << QLatin1String("<br/><br/>");
    msg=orig;

    for (const QString &sep: constSeps) {
        QStringList parts=orig.split(sep);
        if (parts.count()>1) {
            msg=parts.takeAt(0);
            sub=parts.join(sep);
            return;
        }
    }
}

void MessageBox::error(QWidget *parent, const QString &message, const QString &title)
{
    QString msg;
    QString sub;
    splitMessage(message, msg, sub);
    QMessageBox box(QMessageBox::Critical, title.isEmpty() ? QObject::tr("Error") : title, msg, QMessageBox::Ok, parent, Qt::Sheet);
    box.setInformativeText(sub);
    //AcceleratorManager::manage(&box);
    box.exec();
}

void MessageBox::information(QWidget *parent, const QString &message, const QString &title)
{
    QString msg;
    QString sub;
    splitMessage(message, msg, sub);
    QMessageBox box(QMessageBox::Information, title.isEmpty() ? QObject::tr("Information") : title, msg, QMessageBox::Ok, parent, Qt::Sheet);
    box.setInformativeText(sub);
    //AcceleratorManager::manage(&box);
    box.exec();
}
#endif

MessageBox::ButtonCode MessageBox::questionYesNoCancel(QWidget *parent, const QString &message, const QString &title,
                               const GuiItem &yesText, const GuiItem &noText, bool showCancel, bool isWarning)
{
    #ifdef Q_OS_MAC
    QString msg;
    QString sub;
    splitMessage(message, msg, sub);
    QMessageBox box(isWarning ? QMessageBox::Warning : QMessageBox::Question, title.isEmpty() ? (isWarning ? QObject::tr("Warning") : QObject::tr("Question")) : title,
                    msg, QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton), parent, Qt::Sheet);
    box.setInformativeText(sub);
    #else
    QMessageBox box(isWarning ? QMessageBox::Warning : QMessageBox::Question, title.isEmpty() ? (isWarning ? QObject::tr("Warning") : QObject::tr("Question")) : title,
                    message, QMessageBox::Yes|QMessageBox::No|(showCancel ? QMessageBox::Cancel : QMessageBox::NoButton), parent);
    #endif

    bool btnIcons=box.style()->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons);
    box.setDefaultButton(isWarning && !showCancel ? QMessageBox::No : QMessageBox::Yes);
    if (!yesText.text.isEmpty()) {
        QAbstractButton *btn=box.button(QMessageBox::Yes);
        btn->setText(yesText.text);
        btn->setIcon(!yesText.icon.isEmpty() && btnIcons ? Icon::get(yesText.icon) : QIcon());
    }
    if (!noText.text.isEmpty()) {
        QAbstractButton *btn=box.button(QMessageBox::No);
        btn->setText(noText.text);
        btn->setIcon(!noText.icon.isEmpty() && btnIcons ? Icon::get(noText.icon) : QIcon());
    }
    AcceleratorManager::manage(&box);
    return -1==box.exec() ? Cancel : map(box.standardButton(box.clickedButton()));
}

namespace MessageBox
{
class YesNoListDialog : public Dialog
{
public:
    YesNoListDialog(QWidget *p) : Dialog(p) { }
    void slotButtonClicked(int b) override {
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
{
    MessageBox::YesNoListDialog *dlg=new MessageBox::YesNoListDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QWidget *wid=new QWidget(dlg);
    QGridLayout *lay=new QGridLayout(wid);
    QLabel *iconLabel=new QLabel(wid);
    int iconSize=Icon::dlgIconSize();
    iconLabel->setFixedSize(iconSize, iconSize);
    switch(type) {
    case Error:
        dlg->setCaption(title.isEmpty() ? QObject::tr("Error") : title);
        dlg->setButtons(Dialog::Ok);
        iconLabel->setPixmap(qApp->style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(iconSize, iconSize));
        break;
    case Question:
        dlg->setCaption(title.isEmpty() ? QObject::tr("Question") : title);
        dlg->setButtons(Dialog::Yes|Dialog::No);
        iconLabel->setPixmap(qApp->style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(iconSize, iconSize));
        break;
    case Warning:
        dlg->setCaption(title.isEmpty() ? QObject::tr("Warning") : title);
        dlg->setButtons(Dialog::Yes|Dialog::No);
        iconLabel->setPixmap(qApp->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize, iconSize));
        break;
    case Information:
        dlg->setCaption(title.isEmpty() ? QObject::tr("Information") : title);
        dlg->setButtons(Dialog::Ok);
        iconLabel->setPixmap(qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(iconSize, iconSize));
        break;
    }
    lay->addWidget(iconLabel, 0, 0, 1, 1);
    QLabel *msgLabel=new QLabel(message, wid);
    msgLabel->setWordWrap(true);
    msgLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    lay->addWidget(msgLabel, 0, 1, 1, 1);
    QListWidget *list=new QListWidget(wid);
    lay->addWidget(list, 1, 0, 1, 2);
    lay->setMargin(0);
    list->insertItems(0, strlist);
    dlg->setMainWidget(wid);
    #ifdef Q_OS_MAC
    dlg->setWindowFlags((dlg->windowFlags()&(~Qt::WindowType_Mask))|Qt::Sheet);
    #endif
    return QDialog::Accepted==dlg->exec() ? Yes : No;
}
