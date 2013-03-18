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

#ifndef DIALOG_H
#define DIALOG_H

#include "config.h"

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDialog>
struct Dialog : public KDialog {
    Dialog(QWidget *parent, const QString &name=QString());
    virtual ~Dialog();
    #ifdef ENABLE_OVERLAYSCROLLBARS
    int exec();
    #endif

    const QSize & configuredSize() const { return cfgSize; }
    void resize(int w, int h) { resize(QSize(w, h)); }
    void resize(const QSize &sz);

private:
    QSize cfgSize;
};
typedef KGuiItem GuiItem;
#else
#include <QDialog>
#include <QMap>
#include <QDebug>

struct GuiItem {
    GuiItem(const QString &t=QString(), const QString &i=QString())
        : text(t), icon(i) {
    }
    QString text;
    QString icon;
};
class QAbstractButton;
class QDialogButtonBox;
class QMenu;

class Dialog : public QDialog {
    Q_OBJECT
    Q_ENUMS(ButtonCode)

public:
    enum ButtonCode
    {
        None    = 0x00000000,
      Help    = 0x00000001, ///< Show Help button. (this button will run the help set with setHelp)
        Default = 0x00000002, ///< Show Default button.
      Ok      = 0x00000004, ///< Show Ok button. (this button accept()s the dialog; result set to QDialog::Accepted)
      Apply   = 0x00000008, ///< Show Apply button.
        Try     = 0x00000010, ///< Show Try button.
      Cancel  = 0x00000020, ///< Show Cancel-button. (this button reject()s the dialog; result set to QDialog::Rejected)
      Close   = 0x00000040, ///< Show Close-button. (this button closes the dialog)
      No      = 0x00000080, ///< Show No button. (this button closes the dialog and sets the result to KDialog::No)
      Yes     = 0x00000100, ///< Show Yes button. (this button closes the dialog and sets the result to KDialog::Yes)
      Reset   = 0x00000200, ///< Show Reset button
        Details = 0x00000400, ///< Show Details button. (this button will show the detail widget set with setDetailsWidget)
      User1   = 0x00001000, ///< Show User defined button 1.
      User2   = 0x00002000, ///< Show User defined button 2.
      User3   = 0x00004000, ///< Show User defined button 3.
      NoDefault = 0x00008000 ///< Used when specifying a default button; indicates that no button should be marked by default.
    };
    Q_DECLARE_FLAGS(ButtonCodes, ButtonCode)
    enum ButtonPopupMode
    {
      InstantPopup = 0,
      DelayedPopup = 1
    };

    Dialog(QWidget *parent, const QString &name=QString());
    virtual ~Dialog();

    void setCaption(const QString &cap) { setWindowTitle(cap); }
    void setButtons(ButtonCodes buttons);
    void setButtonText(ButtonCode button, const QString &text);
    void setButtonGuiItem(ButtonCode button, const GuiItem &item);
    void setButtonMenu(ButtonCode button, QMenu *menu, ButtonPopupMode popupmode=InstantPopup);
    void enableButton(ButtonCode button, bool enable);
    void enableButtonOk(bool enable) { enableButton(Ok, enable); }
    bool isButtonEnabled(ButtonCode button);
    void setMainWidget(QWidget *widget);
    virtual void slotButtonClicked(int button);
    QWidget * mainWidget() { return mw; }

    #ifdef ENABLE_OVERLAYSCROLLBARS
    int exec();
    #endif

    const QSize & configuredSize() const { return cfgSize; }
    void resize(int w, int h) { resize(QSize(w, h)); }
    void resize(const QSize &sz);

private Q_SLOTS:
    void buttonPressed(QAbstractButton *button);

private:
    void create();
    QAbstractButton *getButton(ButtonCode button);

private:
    int buttonTypes;
    QWidget *mw;
    QDialogButtonBox *buttonBox;
    QMap<ButtonCode, QAbstractButton *> userButtons;
    QSize cfgSize;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Dialog::ButtonCodes)

#endif

#endif
