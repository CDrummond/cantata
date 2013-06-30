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

#include "searchwidget.h"
#include "icon.h"
#include "toolbutton.h"
#include "localize.h"
#include <QBoxLayout>

SearchWidget::SearchWidget(QWidget *p)
     : QWidget(p)
{
    QBoxLayout *l=new QBoxLayout(QBoxLayout::LeftToRight, this);
    l->setMargin(0);
    edit=new LineEdit(this);
    edit->setPlaceholderText(i18n("Search..."));
    l->addWidget(edit);
    ToolButton *closeButton=new ToolButton(this);
    closeButton->setToolTip(i18n("Close Search Bar"));
    l->addWidget(closeButton);
    Icon icon=Icon("dialog-close");
    if (icon.isNull()) {
        icon=Icon("window-close");
    }
    closeButton->setIcon(icon);
    Icon::init(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(edit, SIGNAL(textChanged(QString)), SIGNAL(textChanged(QString)));
    connect(edit, SIGNAL(returnPressed()), SIGNAL(returnPressed()));
}

void SearchWidget::toggle()
{
    if (isVisible()) {
        close();
    } else {
        show();
    }
}

void SearchWidget::close()
{
    edit->setText(QString());
    setVisible(false);
}
