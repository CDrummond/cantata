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

#include "dialog.h"
#include "icon.h"
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>
#include <QtGui/QBoxLayout>

static QDialogButtonBox::StandardButton mapType(int btn) {
    switch (btn) {
    case Dialog::Help:   return QDialogButtonBox::Help;
    case Dialog::Ok:     return QDialogButtonBox::Ok;
    case Dialog::Apply:  return QDialogButtonBox::Apply;
    case Dialog::Cancel: return QDialogButtonBox::Cancel;
    case Dialog::Close:  return QDialogButtonBox::Close;
    case Dialog::No:     return QDialogButtonBox::No;
    case Dialog::Yes:    return QDialogButtonBox::Yes;
    case Dialog::Reset:  return QDialogButtonBox::Reset;
    default:             return QDialogButtonBox::NoButton;
    }
}

void Dialog::setButtons(ButtonCodes buttons)
{
    if (buttonBox) {
        return;
    }
    QFlags<QDialogButtonBox::StandardButton> btns;
    if (buttons&Help) {
        btns|=QDialogButtonBox::Help;
    }
    if (buttons&Ok) {
        btns|=QDialogButtonBox::Ok;
    }
    if (buttons&Apply) {
        btns|=QDialogButtonBox::Apply;
    }
    if (buttons&Cancel) {
        btns|=QDialogButtonBox::Cancel;
    }
    if (buttons&Close) {
        btns|=QDialogButtonBox::Close;
    }
    if (buttons&No) {
        btns|=QDialogButtonBox::No;
    }
    if (buttons&Yes) {
        btns|=QDialogButtonBox::Yes;
    }
    if (buttons&Reset) {
        btns|=QDialogButtonBox::Reset;
    }

    buttonTypes=(int)btns;
    buttonBox = new QDialogButtonBox(btns, Qt::Horizontal, this);

    if (buttons&User3) {
        QPushButton *button=new QPushButton(this);
        userButtons.insert(User3, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }
    if (buttons&User2) {
        QPushButton *button=new QPushButton(this);
        userButtons.insert(User2, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }
    if (buttons&User1) {
        QPushButton *button=new QPushButton(this);
        userButtons.insert(User1, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }

    if (mw && buttonBox) {
        create();
    }
}

void Dialog::setButtonText(ButtonCode button, const QString &text)
{
    QAbstractButton *b=getButton(button);
    if (b) {
        b->setText(text);
    }
}

void Dialog::setButtonGuiItem(ButtonCode button, const GuiItem &item)
{
    QAbstractButton *b=getButton(button);
    if (b) {
        b->setText(item.text);
        if (!item.icon.isEmpty()) {
            b->setIcon(Icon(item.icon));
        }
    }
}

void Dialog::setButtonMenu(ButtonCode button, QMenu *menu, ButtonPopupMode popupmode)
{
    Q_UNUSED(popupmode)
    QAbstractButton *b=getButton(button);
    if (b) {
        qobject_cast<QPushButton *>(b)->setMenu(menu);
    }
}

void Dialog::enableButton(ButtonCode button, bool enable)
{
    QAbstractButton *b=getButton(button);
    if (b) {
        b->setEnabled(enable);
    }
}

bool Dialog::isButtonEnabled(ButtonCode button)
{
    QAbstractButton *b=getButton(button);
    return b ? b->isEnabled() : false;
}

void Dialog::setMainWidget(QWidget *widget)
{
    if (mw) {
        return;
    }
    mw=widget;
    if (mw && buttonBox) {
        create();
    }
}

void Dialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: accept(); break;
    case Cancel: reject(); break;
    case Close: reject(); break;
    default: break;
    }
}

void Dialog::buttonPressed(QAbstractButton *button)
{
    if (buttonTypes&QDialogButtonBox::Help && button==buttonBox->button(QDialogButtonBox::Help)) {
        slotButtonClicked(Help);
    } else if (buttonTypes&QDialogButtonBox::Ok && button==buttonBox->button(QDialogButtonBox::Ok)) {
        slotButtonClicked(Ok);
    } else if (buttonTypes&QDialogButtonBox::Apply && button==buttonBox->button(QDialogButtonBox::Apply)) {
        slotButtonClicked(Apply);
    } else if (buttonTypes&QDialogButtonBox::Cancel && button==buttonBox->button(QDialogButtonBox::Cancel)) {
        slotButtonClicked(Cancel);
    } else if (buttonTypes&QDialogButtonBox::Close && button==buttonBox->button(QDialogButtonBox::Close)) {
        slotButtonClicked(Close);
    } else if (buttonTypes&QDialogButtonBox::No && button==buttonBox->button(QDialogButtonBox::No)) {
        slotButtonClicked(No);
    } else if (buttonTypes&QDialogButtonBox::Yes && button==buttonBox->button(QDialogButtonBox::Yes)) {
        slotButtonClicked(Yes);
    } else if (buttonTypes&QDialogButtonBox::Reset && button==buttonBox->button(QDialogButtonBox::Reset)) {
        slotButtonClicked(Reset);
    } else if (userButtons.contains(User1) && userButtons[User1]==button) {
        slotButtonClicked(User1);
    } else if (userButtons.contains(User2) && userButtons[User2]==button) {
        slotButtonClicked(User2);
    } else if (userButtons.contains(User3) && userButtons[User3]==button) {
        slotButtonClicked(User3);
    }
}

void Dialog::create()
{
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->addWidget(mw);
    layout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
}

QAbstractButton *Dialog::getButton(ButtonCode button)
{
    QDialogButtonBox::StandardButton mapped=mapType(button);
    QAbstractButton *b=QDialogButtonBox::NoButton==mapped ? 0 : buttonBox->button(mapped);
    if (!b && userButtons.contains(button)) {
        b=userButtons[button];
    }
    return b;
}
