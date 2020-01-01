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

#ifndef SEARRCHWIDGET_H
#define SEARRCHWIDGET_H

#include "support/lineedit.h"
#include "toolbutton.h"
#include "support/squeezedtextlabel.h"
#include "selectorlabel.h"
#include <QSet>
#include <QList>
#include <QPair>

class SearchWidget : public QWidget
{
    Q_OBJECT
public:
    struct Category
    {
        Category(const QString &txt=QString(), const QString &f=QString(), const QString &tt=QString())
            : text(txt), field(f), toolTip(tt) {

        }

        QString text;
        QString field;
        QString toolTip;
    };

    SearchWidget(QWidget *p, int extraSpace=0);
    ~SearchWidget() override { }

    void setText(const QString &t) { edit->setText(t); }
    QString text() const { return edit->text(); }
    QString category() const { return cat ? cat->itemData(cat->currentIndex()) : QString(); }
    void setFocus() { edit->setFocus(); }
    bool hasFocus() const { return edit->hasFocus() || (closeButton && closeButton->hasFocus()); }
    bool isActive() const { return widgetIsActive; }
    void setPermanent();
    void setCategories(const QList<Category> &categories);
    void setCategory(const QString &id);

Q_SIGNALS:
    void textChanged(const QString &);
    void returnPressed();
    void active(bool);

public Q_SLOTS:
    void toggle();
    void clear() { edit->clear(); }
    void activate(const QString &text=QString());
    void show() { setVisible(true); }
    void close();

private Q_SLOTS:
    void categoryActivated(int c);

private:
    SelectorLabel *cat;
    LineEdit *edit;
    ToolButton *closeButton;
    bool widgetIsActive;
};

#endif
