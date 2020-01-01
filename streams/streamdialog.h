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

#ifndef STREAMDIALOG_H
#define STREAMDIALOG_H

#include <QSet>
#include <QCheckBox>
#include "support/dialog.h"
#include "support/lineedit.h"

class QLabel;
class BuddyLabel;

class StreamDialog : public Dialog
{
    Q_OBJECT

public:
    StreamDialog(QWidget *parent, bool addToPlayQueue=false);

    void setEdit(const QString &editName, const QString &editUrl);
    QString name() const { return nameEntry->text().trimmed(); }
    QString url() const { return urlEntry->text().trimmed(); }
    bool save() const { return !saveCheckbox || saveCheckbox->isChecked(); }

private Q_SLOTS:
    void changed();

private:
    QString prevName;
    QString prevUrl;
    QCheckBox *saveCheckbox;
    LineEdit *nameEntry;
    LineEdit *urlEntry;
    BuddyLabel *nameLabel;
    QLabel *statusText;
    QSet<QString> urlHandlers;
};

#endif
