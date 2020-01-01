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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include "dialog.h"
#include <QMessageBox>

namespace MessageBox {
    enum ButtonCode {
        Yes=QMessageBox::Yes,
        No=QMessageBox::No,
        Cancel=QMessageBox::Cancel
    };
    enum Type {
        Error = QMessageBox::Critical,
        Question = QMessageBox::Question,
        Warning = QMessageBox::Warning,
        Information = QMessageBox::Information
    };

    extern ButtonCode questionYesNoCancel(QWidget *parent, const QString &message, const QString &title=QString(),
                                          const GuiItem &yesText=StdGuiItem::yes(), const GuiItem &noText=StdGuiItem::no(), bool showCancel=true, bool isWarning=false);
    inline ButtonCode questionYesNo(QWidget *parent, const QString &message, const QString &title=QString(), const GuiItem &yesText=StdGuiItem::yes(), const GuiItem &noText=StdGuiItem::no()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, false);
    }
    inline ButtonCode warningYesNoCancel(QWidget *parent, const QString &message, const QString &title=QString(),
                               const GuiItem &yesText=StdGuiItem::yes(), const GuiItem &noText=StdGuiItem::no()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, true, true);
    }
    inline ButtonCode warningYesNo(QWidget *parent, const QString &message, const QString &title=QString(), const GuiItem &yesText=StdGuiItem::yes(), const GuiItem &noText=StdGuiItem::no()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, false, true);
    }
    #ifdef Q_OS_MAC
    extern void error(QWidget *parent, const QString &message, const QString &title=QString());
    extern void information(QWidget *parent, const QString &message, const QString &title=QString());
    #else
    inline void error(QWidget *parent, const QString &message, const QString &title=QString()) {
        QMessageBox::critical(parent, title.isEmpty() ? QObject::tr("Error") : title, message);
    }
    inline void information(QWidget *parent, const QString &message, const QString &title=QString()) {
        QMessageBox::information(parent, title.isEmpty() ? QObject::tr("Information") : title, message);
    }
    #endif
    extern ButtonCode msgListEx(QWidget *parent, Type type, const QString &message, const QStringList &strlist, const QString &title=QString());
    inline void errorListEx(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString()) {
        msgListEx(parent, Error, message, strlist, title);
    }
    inline void errorList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString()) {
        msgListEx(parent, Error, message, strlist, title);
    }
    inline ButtonCode questionYesNoList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString()) {
        return msgListEx(parent, Question, message, strlist, title);
    }
    inline ButtonCode warningYesNoList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString()) {
        return msgListEx(parent, Warning, message, strlist, title);
    }
    inline void informationList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString()) {
        msgListEx(parent, Information, message, strlist, title);
    }
}

#endif
