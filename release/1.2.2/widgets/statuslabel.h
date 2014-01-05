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

#ifndef STATUSLABEL_H
#define STATUSLABEL_H

#include <QFrame>
#include "squeezedtextlabel.h"

class QLabel;

class StatusLabel : public QFrame
{
public:
    enum Type {
        None,
        Error,
        Locked
    };
    
    StatusLabel(QWidget *p);
    virtual ~StatusLabel() { }

    void setType(Type t);
    void setText(const QString &t) { text->setText(t); }
    
private:
    Type type;
    QLabel *icon;
    SqueezedTextLabel *text;
};

#endif
