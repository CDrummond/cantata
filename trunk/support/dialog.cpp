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

#ifdef ENABLE_OVERLAYSCROLLBARS
int Dialog::exec()
{
    QWidget *win=parentWidget() ? parentWidget()->window() : 0;
    bool wasGl=win ? win->testAttribute(Qt::WA_GroupLeader) : false;
    if (win && !wasGl) {
        win->setAttribute(Qt::WA_GroupLeader, true);
    }
    int rv=QDialog::exec();
    if (win && !wasGl) {
        win->setAttribute(Qt::WA_GroupLeader, false);
    }
    return rv;
}
#endif

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>

Dialog::Dialog(QWidget *parent, const QString &name, const QSize &defSize)
    : KDialog(parent)
{
    if (!name.isEmpty()) {
        setObjectName(name);
        KConfigGroup cfg(KGlobal::config(), name);
        cfgSize=cfg.readEntry("size", QSize());
        if (!cfgSize.isEmpty()) {
            KDialog::resize(cfgSize);
        } else if (!defSize.isEmpty()) {
            KDialog::resize(defSize);
        }
    }
}

Dialog::~Dialog()
{
    if (!objectName().isEmpty() && size()!=cfgSize) {
        KConfigGroup cfg(KGlobal::config(), objectName());
        cfg.writeEntry("size", size());
        KGlobal::config()->sync();
    }
}

void Dialog::resize(const QSize &sz)
{
    if (cfgSize.isEmpty()) {
        KDialog::resize(sz);
        cfgSize=sz;
    }
}

#else
#include "icon.h"
#include "acceleratormanager.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QBoxLayout>
#include <QSettings>
#include <QStyle>

namespace StdGuiItem {
GuiItem ok() { return GuiItem(i18n("&Ok"), "dialog-ok"); }
GuiItem cancel() { return GuiItem(i18n("&Cancel"), "dialog-cancel"); }
GuiItem yes() { return GuiItem(i18n("&Yes"), "dialog-ok"); }
GuiItem no() { return GuiItem(i18n("&No"), "process-stop"); }
GuiItem discard() { return GuiItem(i18n("&Discard"), "edit-clear"); }
GuiItem save() { return GuiItem(i18n("&Save"), "document-save"); }
GuiItem apply() { return GuiItem(i18n("&Apply"), "dialog-ok-apply"); }
GuiItem close() { return GuiItem(i18n("&Close"), "window-close"); }
GuiItem help() { return GuiItem(i18n("&Help"), "help-contents"); }
GuiItem overwrite() { return GuiItem(i18n("&Overwrite")); }
GuiItem reset() { return GuiItem(i18n("&Reset"), "edit-undo"); }
GuiItem cont() { return GuiItem(i18n("&Continue"), "arrow-right"); }
GuiItem del() { return GuiItem(i18n("&Delete"), "edit-delete"); }
GuiItem stop() { return GuiItem(i18n("&Stop"), "process-stop"); }
GuiItem remove() { return   GuiItem(i18n("&Remove"), "list-remove"); }
GuiItem back(bool useRtl) { return GuiItem(i18n("&Previous"), useRtl && Qt::RightToLeft==QApplication::layoutDirection() ? "go-next" : "go-previous"); }
GuiItem forward(bool useRtl) { return GuiItem(i18n("&Next"), useRtl && Qt::RightToLeft==QApplication::layoutDirection() ? "go-previous" : "go-next"); }

QSet<QString> standardNames()
{
    static QSet<QString> names;
    if (names.isEmpty()) {
        QStringList strings = QStringList() << ok().text << cancel().text << yes().text << no().text << discard().text << save().text << apply().text
                                            << close().text << help().text << overwrite().text << reset().text << cont().text << del().text
                                            << stop().text << remove().text << back().text << forward().text;

        foreach (QString s, strings) {
            names.insert(s.remove("&"));
        }
    }
    return names;
}

}

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

Dialog::Dialog(QWidget *parent, const QString &name, const QSize &defSize)
    : QDialog(parent)
    , defButton(0)
    , buttonTypes(0)
    , mw(0)
    , buttonBox(0)
    , managedAccels(false)
{
    if (!name.isEmpty()) {
        setObjectName(name);
        QSettings cfg;
        cfg.beginGroup(name);
        cfgSize=cfg.contains("size") ? cfg.value("size").toSize() : QSize();
        if (!cfgSize.isEmpty()) {
            QDialog::resize(cfgSize);
        } else if (!defSize.isEmpty()) {
            QDialog::resize(defSize);
        }
    }
}

Dialog::~Dialog()
{
    if (!objectName().isEmpty() && size()!=cfgSize) {
        QSettings cfg;
        cfg.beginGroup(objectName());
        cfg.setValue("size", size());
        cfg.sync();
    }
}

void Dialog::resize(const QSize &sz)
{
    if (cfgSize.isEmpty()) {
        QDialog::resize(sz);
        cfgSize=sz;
    }
}

void Dialog::setButtons(ButtonCodes buttons)
{
    if (buttonBox && buttons==buttonTypes) {
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
    bool needToCreate=true;
    if (buttonBox) {
        needToCreate=false;
        buttonBox->clear();
        buttonBox->setStandardButtons(btns);
    } else {
        buttonBox = new QDialogButtonBox(btns, Qt::Horizontal, this);
    }

    if (buttons&Help) {
        setButtonGuiItem(QDialogButtonBox::Help, StdGuiItem::help());
    }
    if (buttons&Ok) {
        setButtonGuiItem(QDialogButtonBox::Ok, StdGuiItem::ok());
    }
    if (buttons&Apply) {
        setButtonGuiItem(QDialogButtonBox::Apply, StdGuiItem::apply());
    }
    if (buttons&Cancel) {
        setButtonGuiItem(QDialogButtonBox::Cancel, StdGuiItem::cancel());
    }
    if (buttons&Close) {
        setButtonGuiItem(QDialogButtonBox::Close, StdGuiItem::close());
    }
    if (buttons&No) {
        setButtonGuiItem(QDialogButtonBox::No, StdGuiItem::no());
    }
    if (buttons&Yes) {
        setButtonGuiItem(QDialogButtonBox::Yes, StdGuiItem::yes());
    }
    if (buttons&Reset) {
        setButtonGuiItem(QDialogButtonBox::Reset, StdGuiItem::reset());
    }

    if (buttons&User3) {
        QPushButton *button=new QPushButton(buttonBox);
        userButtons.insert(User3, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }
    if (buttons&User2) {
        QPushButton *button=new QPushButton(buttonBox);
        userButtons.insert(User2, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }
    if (buttons&User1) {
        QPushButton *button=new QPushButton(buttonBox);
        userButtons.insert(User1, button);
        buttonBox->addButton(button, QDialogButtonBox::ActionRole);
    }

    if (needToCreate && mw && buttonBox) {
        create();
    }
}

void Dialog::setDefaultButton(ButtonCode button)
{
    QAbstractButton *b=getButton(button);
    if (b) {
        qobject_cast<QPushButton *>(b)->setDefault(true);
    }
    defButton=button;
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
        if (!item.icon.isEmpty() && style()->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons)) {
            b->setIcon(Icon(item.icon));
        }
    }
}

void Dialog::setButtonGuiItem(QDialogButtonBox::StandardButton button, const GuiItem &item)
{
    QAbstractButton *b=buttonBox->button(button);
    if (b) {
        b->setText(item.text);
        if (!item.icon.isEmpty() && style()->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons)) {
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

void Dialog::showEvent(QShowEvent *e)
{
    if (!managedAccels) {
        AcceleratorManager::manage(this);
        managedAccels=true;
    }
    if (defButton) {
        setDefaultButton((ButtonCode)defButton);
    }
    QDialog::showEvent(e);
}

#endif

