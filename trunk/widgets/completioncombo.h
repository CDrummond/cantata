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

#ifndef COMPLETION_COMBO_H
#define COMPLETION_COMBO_H

// #ifdef ENABLE_KDE_SUPPORT
// #include <KDE/KComboBox>
// #include <KDE/KLineEdit>
// #include <KDE/KGlobalSettings>
// #include <QAbstractItemView>
// 
// class CompletionCombo : public KComboBox
// {
// public:
//     CompletionCombo(QWidget *p)
//         : KComboBox(p) {
//         completionObject()->setIgnoreCase(true);
//         setCompletionMode(KGlobalSettings::CompletionPopup);
//         setEditable(true);
//         setAutoCompletion(false);
//         setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
//         setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
//         view()->setTextElideMode(Qt::ElideRight);
//     }
// 
//     void setEnabled(bool e) {
//         KComboBox::setEnabled(e);
//         qobject_cast<KLineEdit*>(lineEdit())->setClearButtonShown(e);
//     }
// 
//     void insertItems(int index, const QStringList &items) {
//         KComboBox::insertItems(index, items);
//         completionObject()->setItems(items);
//     }
// 
//     void setText(const QString &text) {
//         qobject_cast<KLineEdit*>(lineEdit())->setText(text);
//     }
// 
//     QString text() const {
//         return qobject_cast<KLineEdit*>(lineEdit())->text();
//     }
// 
//     void setPlaceholderText(const QString &text) {
//         qobject_cast<KLineEdit*>(lineEdit())->setPlaceholderText(text);
//     }
// };
// #else
#include "combobox.h"
#include "lineedit.h"
#include <QAbstractItemView>

class CompletionCombo : public ComboBox
{
public:
    CompletionCombo(QWidget *p)
        : ComboBox(p) {
        #ifdef ENABLE_KDE_SUPPORT
        completionObject()->setIgnoreCase(true);
        setCompletionMode(KGlobalSettings::CompletionPopup);
        setAutoCompletion(false);
        #endif
        setEditable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
        view()->setTextElideMode(Qt::ElideRight);
    }

    #ifdef ENABLE_KDE_SUPPORT
    void setEnabled(bool e) {
        ComboBox::setEnabled(e);
        qobject_cast<KLineEdit*>(lineEdit())->setClearButtonShown(e);
    }

    void insertItems(int index, const QStringList &items) {
        ComboBox::insertItems(index, items);
        completionObject()->setItems(items);
    }
    #endif
    
    void setText(const QString &text) {
        if (lineEdit()) qobject_cast<QLineEdit*>(lineEdit())->setText(text);
    }

    QString text() const {
        return lineEdit() ? qobject_cast<QLineEdit*>(lineEdit())->text() : QString();
    }

    void setPlaceholderText(const QString &text) {
        if (lineEdit()) qobject_cast<QLineEdit*>(lineEdit())->setPlaceholderText(text);
    }
};

#endif
