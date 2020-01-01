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

#ifndef SELECTOR_LABEL_H
#define SELECTOR_LABEL_H

#include <QLabel>
#include <QString>
#include <QMenu>

class QMenu;
class SelectorLabel : public QLabel
{
    Q_OBJECT
public:
    SelectorLabel(QWidget *p);
    void setUseArrow(bool a) { useArrow=a; }
    void clear() { if (menu) menu->clear(); }
    void addItem(const QString &text, const QString &data, const QString &tt=QString());
    bool event(QEvent *e) override;
    int currentIndex() const { return current; }
    void setCurrentIndex(int v);
    QString itemData(int index) const;
    QAction * action(int index) const;
    int count() const { return menu ? menu->actions().count() : 0; }
    void setColor(const QColor &col) { textColor=col; }
    void setBold(bool b) { bold=b; }

Q_SIGNALS:
    void activated(int);

private Q_SLOTS:
    void itemSelected();

private:
    QColor textColor;
    int current;
    bool useArrow;
    bool bold;
    QMenu *menu;
};

#endif
