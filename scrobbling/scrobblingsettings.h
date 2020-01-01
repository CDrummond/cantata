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

#ifndef SCROBBLING_SETTINGS_H
#define SCROBBLING_SETTINGS_H

#include "ui_scrobblingsettings.h"

class LineEdit;
class QPushButton;
class QLabel;

class ScrobblingSettings : public QWidget, public Ui::ScrobblingSettings
{
    Q_OBJECT

public:
    ScrobblingSettings(QWidget *parent);

    void load();

public Q_SLOTS:
    void save();

private Q_SLOTS:
    void showStatus(bool status);
    void showError(const QString &msg);
    void controlLoginButton();
    void scrobblerChanged();
};

#endif
