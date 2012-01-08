/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef STREAMDIALOG_H
#define STREAMDIALOG_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDialog>
#else
#include <QtGui/QDialog>
class QDialogButtonBox;
#endif
#include "lineedit.h"
#include <QtGui/QComboBox>

#ifdef ENABLE_KDE_SUPPORT
class StreamDialog : public KDialog
#else
class StreamDialog : public QDialog
#endif
{
    Q_OBJECT

public:
    StreamDialog(const QStringList &categories, QWidget *parent);

    void setEdit(const QString &cat, const QString &editName, const QString &editUrl);

    QString name() const { return nameEntry->text().trimmed(); }
    QString url() const { return urlEntry->text().trimmed(); }
    QString category() const { return catCombo->currentText().trimmed(); }

private Q_SLOTS:
    void changed();

private:
    QString prevName;
    QString prevUrl;
    QString prevCat;
#ifndef ENABLE_KDE_SUPPORT
    QDialogButtonBox *buttonBox;
#endif
    LineEdit *nameEntry;
    LineEdit *urlEntry;
    QComboBox *catCombo;
};

#endif
