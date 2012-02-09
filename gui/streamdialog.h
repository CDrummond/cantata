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
#include "completioncombo.h"

class QLabel;
#ifdef ENABLE_KDE_SUPPORT
class QPushButton;

class StreamDialog : public KDialog
#else
class StreamDialog : public QDialog
#endif
{
    Q_OBJECT

public:
    StreamDialog(const QStringList &categories, const QStringList &genres, const QSet<QString> &uh, QWidget *parent);

    void setEdit(const QString &cat, const QString &editName, const QString &editGenre, const QString &editIconName, const QString &editUrl);

    QString name() const { return nameEntry->text().trimmed(); }
    QString url() const { return urlEntry->text().trimmed(); }
    QString category() const { return catCombo->currentText().trimmed(); }
    QString genre() const { return genreCombo->currentText().trimmed(); }
    #ifdef ENABLE_KDE_SUPPORT
    QString icon() const { return iconName; }
    #endif

private Q_SLOTS:
    void changed();
    #ifdef ENABLE_KDE_SUPPORT
    void setIcon();
    #endif

#ifdef ENABLE_KDE_SUPPORT
private:
    void setIcon(const QString &icn);
#endif

private:
    QString prevName;
    QString prevUrl;
    QString prevCat;
    QString prevGenre;
    #ifdef ENABLE_KDE_SUPPORT
    QString prevIconName;
    QString iconName;
    QPushButton *iconButton;
    #else
    QDialogButtonBox *buttonBox;
    #endif
    LineEdit *nameEntry;
    LineEdit *urlEntry;
    CompletionCombo *catCombo;
    CompletionCombo *genreCombo;
    QLabel *statusText;
    QSet<QString> urlHandlers;
};

#endif
