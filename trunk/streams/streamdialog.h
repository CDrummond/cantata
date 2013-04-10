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

#ifndef STREAMDIALOG_H
#define STREAMDIALOG_H

#include <QSet>
#include "combobox.h"
#include "dialog.h"
#include "lineedit.h"
#include "completioncombo.h"

class QLabel;
class BuddyLabel;

class IconCombo : public ComboBox
{
public:
    IconCombo(QWidget *p) : ComboBox(p) { }
    void load();
    void showEvent(QShowEvent *e);
};

class StreamDialog : public Dialog
{
    Q_OBJECT

public:
    StreamDialog(const QStringList &categories, const QStringList &genres, QWidget *parent, bool addToPlayQueue=false);

    void setEdit(const QString &cat, const QString &editName, const QString &editGenre, const QString &editIconName, const QString &editUrl);
    QString name() const { return nameEntry->text().trimmed(); }
    QString url() const { return urlEntry->text().trimmed(); }
    QString category() const { return catCombo->currentText().trimmed(); }
    QString genre() const { return genreCombo->currentText().trimmed(); }
    QString icon() const { return iconCombo ? iconCombo->itemData(iconCombo->currentIndex()).toString() : prevIconName; }
    bool save() const { return !saveCombo || 1==saveCombo->currentIndex(); }

private Q_SLOTS:
    void saveComboChanged();
    void changed();

private:
    void setWidgetVisiblity();

private:
    QString prevName;
    QString prevUrl;
    QString prevCat;
    QString prevGenre;
    QString prevIconName;
    QComboBox *saveCombo;
    IconCombo *iconCombo;
    LineEdit *nameEntry;
    LineEdit *urlEntry;
    CompletionCombo *catCombo;
    CompletionCombo *genreCombo;
    BuddyLabel *nameLabel;
    BuddyLabel *catLabel;
    BuddyLabel *genreLabel;
    BuddyLabel *iconLabel;
    QLabel *multipleGenresText;
    QLabel *statusText;
    QSet<QString> urlHandlers;
};

#endif
