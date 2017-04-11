/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef ICON_H
#define ICON_H

#include <QIcon>

class QToolButton;

class Icon : public QIcon
{
public:
    explicit Icon(const QString &icon) : QIcon(QIcon::fromTheme(icon)) { }
    Icon(const QIcon &i) : QIcon(i) { }
    Icon(const QStringList &names);
    Icon() { }

    static int stdSize(int s);
    static int dlgIconSize();
    static void init(QToolButton *btn, bool setFlat=true);
    static Icon getMediaIcon(const QString &name);
    static QString currentTheme() { return QIcon::themeName(); }
    static Icon create(const QString &name, const QList<int> &sizes, bool andSvg=false);

    enum Std {
        Close,
        Clear
    };
    static QPixmap getScaledPixmap(const QIcon &icon, int w, int h, int base);
    QPixmap getScaledPixmap(int w, int h, int base) const { return getScaledPixmap(*this, w, h, base); }
};

#endif
