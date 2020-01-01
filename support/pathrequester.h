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

#ifndef PATH_REQUESER_H
#define PATH_REQUESER_H

#include "lineedit.h"
#include "flattoolbutton.h"
#include <QDir>

class PathRequester : public QWidget
{
    Q_OBJECT
public:
    static void setIcon(const QIcon &icn);
    PathRequester(QWidget *parent);
    ~PathRequester() override { }

    QString text() const { return QDir::fromNativeSeparators(edit->text()); }
    void setText(const QString &t) { edit->setText(QDir::toNativeSeparators(t)); }
    void setButtonVisible(bool v) { btn->setVisible(v); }
    void setFocus() { edit->setFocus(); }
    void setDirMode(bool m) { dirMode=m; }
    LineEdit * lineEdit() const { return edit; }
    QToolButton * button() const { return btn; }
    void setFilter(const QString &f) { filter=f; }

public Q_SLOTS:
    void setEnabled(bool e);

Q_SIGNALS:
    void textChanged(const QString &);

private Q_SLOTS:
    void choose();

private:
    LineEdit *edit;
    FlatToolButton *btn;
    bool dirMode;
    QString filter;
};

#endif
