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

#include "searchwidget.h"
#include "support/monoicon.h"
#include "support/utils.h"
#include "toolbutton.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSpacerItem>

class EscKeyEventHandler : public QObject
{
public:
    EscKeyEventHandler(SearchWidget *v) : QObject(v), view(v) { }
protected:
    bool eventFilter(QObject *obj, QEvent *event) override
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

SearchWidget::SearchWidget(QWidget *p, int extraSpace)
     : QWidget(p)
     , cat(nullptr)
     , widgetIsActive(false)
{
    QHBoxLayout *l=new QHBoxLayout(this);
    #ifdef Q_OS_MAC
    l->setSpacing(2);
    l->setContentsMargins(0, 1, 1, 1+extraSpace);
    bool closeOnLeft=true;
    #else
    l->setSpacing(0);
    l->setContentsMargins(0, 1, 0, 1+extraSpace);
    bool closeOnLeft=Utils::Unity==Utils::currentDe();
    #endif
    edit=new LineEdit(this);
    edit->setPlaceholderText(tr("Search..."));
    closeButton=new ToolButton(this);
    closeButton->setToolTip(tr("Close Search Bar")+QLatin1String(" (")+QKeySequence(Qt::Key_Escape).toString()+QLatin1Char(')'));

    if (closeOnLeft) {
        l->addWidget(closeButton);
        l->addWidget(edit);
    } else {
        l->addWidget(edit);
        l->addWidget(closeButton);
    }

    closeButton->setIcon(MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed));
    Icon::init(closeButton);
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(edit, SIGNAL(textChanged(QString)), SIGNAL(textChanged(QString)));
    connect(edit, SIGNAL(returnPressed()), SIGNAL(returnPressed()));
    installEventFilter(new EscKeyEventHandler(this));;
    setTabOrder(edit, closeButton);
}

void SearchWidget::setPermanent()
{
    show();
    setFocus();
    closeButton->setVisible(false);
    closeButton->deleteLater();
    closeButton=nullptr;
    layout()->setSpacing(0);
}

void SearchWidget::setCategories(const QList<Category> &categories)
{
    QString currentCat;
    if (!cat) {
        cat=new SelectorLabel(this);
        QHBoxLayout *l=static_cast<QHBoxLayout *>(layout());
        l->insertWidget(0, cat);
        l->setSpacing(qMin(4, Utils::layoutSpacing(this)));
        connect(cat, SIGNAL(activated(int)), SIGNAL(returnPressed()));
        connect(cat, SIGNAL(activated(int)), this, SLOT(categoryActivated(int)));
        setTabOrder(cat, edit);
        cat->setFixedHeight(edit->height());
    } else {
        currentCat=category();
        if (!currentCat.isEmpty()) {
            cat->blockSignals(true);
        }
    }

    cat->clear();

    for (const Category &c: categories) {
        cat->addItem(c.text, c.field, c.toolTip);
    }

    if (!currentCat.isEmpty()) {
        for (int i=0; i<cat->count(); ++i) {
            if (cat->itemData(i)==currentCat) {
                cat->setCurrentIndex(i);
                cat->blockSignals(false);
                setToolTip(cat->action(i)->toolTip());
                return;
            }
        }
    }
    cat->blockSignals(false);
    cat->setCurrentIndex(0);
}

void SearchWidget::setCategory(const QString &id)
{
    if (!cat || id.isEmpty()) {
        return;
    }
    for (int i=0; i<cat->count(); ++i) {
        if (cat->itemData(i)==id) {
            cat->setCurrentIndex(i);
            setToolTip(cat->action(i)->toolTip());
            return;
        }
    }
}

void SearchWidget::toggle()
{
    if (isVisible()) {
        close();
    } else {
        activate();
    }
}

void SearchWidget::activate(const QString &text)
{
    bool wasActive=widgetIsActive;
    widgetIsActive=true;
    if (!text.isEmpty()) {
        edit->setText(text);
    } else {
        edit->selectAll();
    }
    show();
    setFocus();
    if (wasActive!=widgetIsActive) {
        emit active(widgetIsActive);
    }
}

void SearchWidget::close()
{
    if (!closeButton) {
        return;
    }
    bool wasActive=widgetIsActive;
    widgetIsActive=false;
    setVisible(false);
    edit->setText(QString());
    if (wasActive!=widgetIsActive) {
        emit active(widgetIsActive);
    }
}

void SearchWidget::categoryActivated(int c)
{
    QAction *a=cat->action(c);
    if (a) {
        setToolTip(a->toolTip());
    }
}

#include "moc_searchwidget.cpp"
