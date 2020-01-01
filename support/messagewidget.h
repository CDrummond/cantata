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

#ifndef MESSAGEWIDGET_H
#define MESSAGEWIDGET_H

#include "kmessagewidget.h"
#include <QList>

class MessageWidget : public KMsgWidget
{
    Q_OBJECT
public:

    MessageWidget(QWidget *parent);
    ~MessageWidget() override;
    void setError(const QString &msg, bool showCloseButton=true) { setMessage(msg, Error, showCloseButton); }
    void setInformation(const QString &msg, bool showCloseButton=true) { setMessage(msg, Information, showCloseButton); }
    void setWarning(const QString &msg, bool showCloseButton=true) { setMessage(msg, Warning, showCloseButton); }
    void setVisible(bool v) override;
    bool isActive() const { return active; }
    void removeAllActions();
    void setActions(const QList<QAction *> acts);
    bool showingError() const { return Error==msgType; }

Q_SIGNALS:
    void visible(bool);

private:
    void setMessage(const QString &msg, MessageType type, bool showCloseButton);

private:
    bool active;
    MessageType msgType;
};

#endif
