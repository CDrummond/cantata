/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MESSAGEWIDGET_H
#define MESSAGEWIDGET_H

#ifdef ENABLE_KDE_SUPPORT
#include <kdeversion.h>
#if KDE_IS_VERSION(4, 7, 0)
#include <KDE/KMessageWidget>
#else // KDE_IS_VERSION(4, 7, 0)
#include "kmessagewidget.h"
#endif // KDE_IS_VERSION(4, 7, 0)
#else // ENABLE_KDE_SUPPORT
#include "kmessagewidget.h"
#endif // ENABLE_KDE_SUPPORT

class MessageWidget : public KMessageWidget
{
    Q_OBJECT
public:
    MessageWidget(QWidget *parent);
    virtual ~MessageWidget();
    void setError(const QString &msg);
    void setVisible(bool v);

Q_SIGNALS:
    void visible(bool);
};

#endif
