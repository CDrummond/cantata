/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <KDialog>
#else
#include <QDialog>
class QDialogButtonBox;
#endif
#include "lineedit.h"
#include <QtGui/QCheckBox>

#ifdef ENABLE_KDE_SUPPORT
class StreamDialog : public KDialog
#else
class StreamDialog : public QDialog
#endif
{
    Q_OBJECT

public:
    StreamDialog(QWidget *parent);

    void setEdit(const QString &editName, const QString &editUrl, bool f);

    QString name() const { return nameEntry->text().trimmed(); }
    QString url() const { return urlEntry->text().trimmed(); }
    bool favorite() const { return fav->isChecked(); }

private Q_SLOTS:
    void changed();

private:
    QString prevName;
    QString prevUrl;
    bool prevFav;
#ifndef ENABLE_KDE_SUPPORT
    QDialogButtonBox *buttonBox;
#endif
    LineEdit *nameEntry;
    LineEdit *urlEntry;
    QCheckBox *fav;
};

#endif
