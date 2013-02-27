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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include "dialog.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMessageBox>
class MessageBox: public KMessageBox {
    static failedList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString());
};
#else
#include <QMessageBox>
#include "localize.h"

namespace MessageBox {
    enum ButtonCode {
        Yes=QMessageBox::Yes,
        No=QMessageBox::No,
        Cancel=QMessageBox::Cancel
    };

    extern ButtonCode questionYesNoCancel(QWidget *parent, const QString &message, const QString &title=QString(),
                               const GuiItem &yesText=GuiItem(), const GuiItem &noText=GuiItem(), bool showCancel=true, bool isWarning=false);
    inline ButtonCode questionYesNo(QWidget *parent, const QString &message, const QString &title=QString(), const GuiItem &yesText=GuiItem(), const GuiItem &noText=GuiItem()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, false);
    }
    inline ButtonCode warningYesNoCancel(QWidget *parent, const QString &message, const QString &title=QString(),
                               const GuiItem &yesText=GuiItem(), const GuiItem &noText=GuiItem()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, true, true);
    }
    inline ButtonCode warningYesNo(QWidget *parent, const QString &message, const QString &title=QString(), const GuiItem &yesText=GuiItem(), const GuiItem &noText=GuiItem()) {
        return questionYesNoCancel(parent, message, title, yesText, noText, false, true);
    }
    inline void error(QWidget *parent, const QString &message, const QString &title=QString()) {
        QMessageBox::warning(parent, title.isEmpty() ? i18n("Error") : title, message);
    }
    inline void information(QWidget *parent, const QString &message, const QString &title=QString()) {
        QMessageBox::information(parent, title.isEmpty() ? i18n("Information") : title, message);
    }
    void failedList(QWidget *parent, const QString &message, const QStringList &strlist, const QString &title=QString());
};
#endif

#endif
