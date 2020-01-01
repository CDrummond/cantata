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

#include "splitlabelwidget.h"
#include "support/squeezedtextlabel.h"
#include <QFormLayout>
#include <QBoxLayout>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater()

SplitLabelWidget::SplitLabelWidget(QWidget *p)
    : QStackedWidget(p)
{
    QWidget *singlePage=new QWidget(this);
    QBoxLayout *singleLayout=new QBoxLayout(QBoxLayout::TopToBottom, singlePage);
    single=new QLabel(singlePage);
    single->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
    single->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    singleLayout->addWidget(single);

    multiplePage=new QWidget(this);
    QFormLayout *multipleLayout=new QFormLayout(multiplePage);
    message=new QLabel(multiplePage);
    multipleLayout->setSpacing(0);
    multipleLayout->setWidget(0, QFormLayout::SpanningRole, message);
    addWidget(singlePage);
    addWidget(multiplePage);
    setCurrentIndex(0);
}

void SplitLabelWidget::setText(const QString &text)
{
    setCurrentIndex(0);
    single->setText(text);
}

void SplitLabelWidget::setText(const QList<QPair<QString, QString> > &details, const QString &msg)
{
    if (details.isEmpty()) {
        setText(msg);
        return;
    }

    setCurrentIndex(1);
    if (details.count()!=labels.count()) {
        if (details.count()<labels.count()) {
            int diff=labels.count()-details.count();
            for (int i=0; i<diff; ++i) {
                QLabel *l=labels.takeLast();
                SqueezedTextLabel *v=values.takeLast();
                REMOVE(l);
                REMOVE(v);
            }
        } else {
            QFormLayout *lay=static_cast<QFormLayout *>(multiplePage->layout());
            int diff=details.count()-labels.count();
            for (int i=0; i<diff; ++i) {
                QLabel *l=new QLabel(multiplePage);
                SqueezedTextLabel *v=new SqueezedTextLabel(multiplePage);
                lay->addRow(l, v);
                labels.append(l);
                values.append(v);
            }
        }
    }

    if (msg.isEmpty()) {
        message->setVisible(false);
    } else {
        message->setVisible(true);
        message->setText(msg);
    }

    for (int i=0; i<details.count(); ++i) {
        labels.at(i)->setText(details.at(i).first);
        values.at(i)->setText(QLatin1String("  ")+details.at(i).second);
    }
}
