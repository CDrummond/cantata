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

#ifndef SPLIT_LABEL_WIDGET
#define SPLIT_LABEL_WIDGET

#include <QStackedWidget>
#include <QList>
#include <QPair>

class QLabel;
class QStackedWidget;
class SqueezedTextLabel;

class SplitLabelWidget : public QStackedWidget
{
public:
    SplitLabelWidget(QWidget *p);
    void setText(const QString &text);
    void setText(const QList<QPair<QString, QString> > &details, const QString &msg=QString());

private:
    QLabel *single;

    QWidget *multiplePage;
    QLabel *message;
    QList<QLabel *> labels;
    QList<SqueezedTextLabel *> values;
};


#endif
