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

#ifndef DIGITALLYIMPORTED_SETTINGS_H
#define DIGITALLYIMPORTED_SETTINGS_H

#include "dialog.h"
#include "ui_digitallyimportedsettings.h"
class LineEdit;
class QComboBox;
class QPushButton;
class QLabel;

class DigitallyImportedSettings : public Dialog, public Ui::DigitallyImportedSettings
{
    Q_OBJECT

public:
    DigitallyImportedSettings(QWidget *parent);

    void show();

private Q_SLOTS:
    void login();
    void loginStatus(bool, const QString &msg);

private:
    bool wasLoggedIn;
    QString prevUser;
    QString prevPass;
    int prevAudioType;
};

#endif
