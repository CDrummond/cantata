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

#ifndef STREAMCATEGORYDIALOG_H
#define STREAMCATEGORYDIALOG_H

#include "dialog.h"
#include "lineedit.h"
#include "combobox.h"
#include <QSet>

class StreamCategoryDialog : public Dialog
{
    Q_OBJECT

public:
    StreamCategoryDialog(const QStringList &categories, QWidget *parent);

    void setEdit(const QString &editName, const QString &editIconName);
    QString name() const { return nameEntry->text().trimmed(); }
    QString icon() const { return iconCombo ? iconCombo->itemData(iconCombo->currentIndex()).toString() : prevIconName; }

private Q_SLOTS:
    void changed();

private:
    QString prevName;
    QString prevIconName;
    ComboBox *iconCombo;
    LineEdit *nameEntry;
    QSet<QString> existingCategories;
};

#endif
