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

#ifndef INTERFACESETTINGS_H
#define INTERFACESETTINGS_H

#include "ui_interfacesettings.h"

class QStringList;

class InterfaceSettings : public QWidget, private Ui::InterfaceSettings
{
    Q_OBJECT

public:
    InterfaceSettings(QWidget *p);
    ~InterfaceSettings() override { }

    void load();
    void save();

    void showEvent(QShowEvent *e) override;
    void showPage(const QString &page);
    QSize sizeHint() const override;

private:
    void addView(const QString &v, const QString &prop);

private Q_SLOTS:
    void playQueueViewChanged();
    void forceSingleClickChanged();
    void enableStartupState();
    void langChanged();
    void styleChanged();
    void viewItemChanged(QListWidgetItem *changedItem);
    void sbAutoHideChanged();
    void setPlayQueueBackgroundOpacityLabel();
    void setPlayQueueBackgroundBlurLabel();
    void enablePlayQueueBackgroundOptions();
    void systemTrayCheckBoxToggled();
    void systemTrayPopupToggled();
    void storeCoversInMpdDirToggled();

private:
    bool loaded;
};

#endif
