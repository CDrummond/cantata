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
#include <QGridLayout>
#include <QKeyEvent>
#include <QKeySequence>

class EscKeyEventHandler : public QObject
{
public:
    EscKeyEventHandler(SearchWidget *v) : QObject(v), view(v) { }
protected:
    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (view->hasFocus() && QEvent::KeyRelease==event->type()) {
            QKeyEvent *keyEvent=static_cast<QKeyEvent *>(event);
            if (Qt::Key_Escape==keyEvent->key() && Qt::NoModifier==keyEvent->modifiers()) {
                view->close();
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    SearchWidget *view;
};

SearchWidget::SearchWidget(QWidget *p)
     : QWidget(p)
     , widgetIsActive(false)
{
    QGridLayout *l=new QGridLayout(this);
    l->setMargin(0);
    l->setSpacing(0);
    label=new SqueezedTextLabel(this);
    edit=new LineEdit(this);
    edit->setPlaceholderText(i18n("Search..."));
    l->addWidget(label, 0, 0, 1, 2);
    l->addWidget(edit, 1, 0);
    closeButton=new ToolButton(this);
    closeButton->setToolTip(i18n("Close Search Bar"));
    l->addWidget(closeButton, 1, 1);
    Icon icon=Icon("dialog-close");
    if (icon.isNull()) {
        icon=Icon("window-close");
    }
    closeButton->setIcon(icon);
    Icon::init(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(edit, SIGNAL(textChanged(QString)), SIGNAL(textChanged(QString)));
    connect(edit, SIGNAL(returnPressed()), SIGNAL(returnPressed()));
    installEventFilter(new EscKeyEventHandler(this));
    label->setVisible(false);
    label->setAlignment(Qt::AlignTop);
    QFont f(font());
    f.setBold(true);
    label->setFont(f);
}

void SearchWidget::setLabel(const QString &s)
{
    label->setText(s);
    label->setVisible(!s.isEmpty());
}

void SearchWidget::toggle()
{
    if (isVisible()) {
        close();
    } else {
        activate();
    }
}

void SearchWidget::activate()
{
    widgetIsActive=true;
    show();
    setFocus();
    emit active(widgetIsActive);
}

void SearchWidget::close()
{
    widgetIsActive=false;
    setVisible(false);
    edit->setText(QString());
    emit active(widgetIsActive);
}
