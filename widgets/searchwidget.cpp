/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/icon.h"
#include "toolbutton.h"
#include "support/localize.h"
#include <QGridLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSpacerItem>

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
     , cat(0)
     , widgetIsActive(false)
{
    QGridLayout *l=new QGridLayout(this);
    l->setMargin(0);
    #ifdef Q_OS_MAC
    l->setSpacing(2);
    bool closeOnLeft=true;
    #else
    l->setSpacing(0);
    bool closeOnLeft=Utils::Unity==Utils::currentDe();
    #endif
    label=new SqueezedTextLabel(this);
    edit=new LineEdit(this);
    edit->setPlaceholderText(i18n("Search..."));
    closeButton=new ToolButton(this);
    closeButton->setToolTip(i18n("Close Search Bar")+QLatin1String(" (")+QKeySequence(Qt::Key_Escape).toString()+QLatin1Char(')'));

    l->addWidget(label, 0, 0, 1, 3);
    if (closeOnLeft) {
        l->addWidget(closeButton, 1, 1);
        l->addWidget(edit, 1, 2);
    } else {
        l->addWidget(edit, 1, 1);
        l->addWidget(closeButton, 1, 2);
    }

    closeButton->setIcon(Icon::std(Icon::Close));
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
    setTabOrder(edit, closeButton);
}

void SearchWidget::setLabel(const QString &s)
{
    label->setText(s);
    label->setVisible(!s.isEmpty());
}

void SearchWidget::setPermanent()
{
    show();
    setFocus();
    closeButton->setVisible(false);
    closeButton->deleteLater();
    closeButton=0;
}

void SearchWidget::setCategories(const QList<QPair<QString, QString> > &categories)
{
    QString currentCat;
    if (!cat) {
        cat=new SelectorLabel(this);
        QGridLayout *l=static_cast<QGridLayout *>(layout());
        l->addWidget(cat, 1, 0);
        connect(cat, SIGNAL(activated(int)), SIGNAL(returnPressed()));
        setTabOrder(cat, edit);
        cat->setFixedHeight(edit->height());
    } else {
        currentCat=category();
        if (!currentCat.isEmpty()) {
            cat->blockSignals(true);
        }
    }

    cat->clear();

    QList<QPair<QString, QString> >::ConstIterator it=categories.constBegin();
    QList<QPair<QString, QString> >::ConstIterator end=categories.constEnd();
    for (; it!=end; ++it) {
        cat->addItem((*it).first, (*it).second);
    }

    if (!currentCat.isEmpty()) {
        for (int i=0; i<cat->count(); ++i) {
            if (cat->itemData(i)==currentCat) {
                cat->setCurrentIndex(i);
                cat->blockSignals(false);
                return;
            }
        }
        cat->blockSignals(false);
        cat->setCurrentIndex(0);
    }
}

void SearchWidget::setCategory(const QString &id)
{
    if (!cat || id.isEmpty()) {
        return;
    }
    for (int i=0; i<cat->count(); ++i) {
        if (cat->itemData(i)==id) {
            cat->setCurrentIndex(i);
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
    bool wasActive=widgetIsActive;
    widgetIsActive=false;
    setVisible(false);
    edit->setText(QString());
    if (wasActive!=widgetIsActive) {
        emit active(widgetIsActive);
    }
}
