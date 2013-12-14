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

/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#ifndef LINEEDIT_H
#define LINEEDIT_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLineEdit>
class LineEdit : public KLineEdit
{
public:
    LineEdit(QWidget *parent = 0) : KLineEdit(parent) { setClearButtonShown(true); }
    virtual ~LineEdit() { }
    void setReadOnly(bool e);
};

#else
#include <QLineEdit>

class QToolButton;

class LineEdit : public QLineEdit
{
    Q_OBJECT

public:
    LineEdit(QWidget *parent = 0);
    virtual ~LineEdit() { }

    void setReadOnly(bool e);

protected:
    void resizeEvent(QResizeEvent *);

private Q_SLOTS:
    void updateCloseButton(const QString &text);

private:
    QToolButton *clearButton;
};

#endif

#endif // LIENEDIT_H

