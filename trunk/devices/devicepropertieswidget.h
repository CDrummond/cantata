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

#ifndef DEVICEPROPERTIESWIDGET_H
#define DEVICEPROPERTIESWIDGET_H

#include "ui_devicepropertieswidget.h"
#include "device.h"

class FilenameSchemeDialog;
class DevicePropertiesWidget : public QWidget, Ui::DevicePropertiesWidget
{
    Q_OBJECT

public:
    enum Properties {
        Prop_Basic      = 0x00,

        Prop_Folder     = 0x01,
        Prop_Covers     = 0x02,
        Prop_Va         = 0x04,
        Prop_Transcoder = 0x08,
        Prop_Cache      = 0x10,

        Prop_All        = 0xFF
    };
    DevicePropertiesWidget(QWidget *parent);
    virtual ~DevicePropertiesWidget() { }
    void update(const QString &path, const QString &coverName, const Device::Options &opts, int props);
    Device::Options settings();
    bool isModified() const { return modified; }
    bool isSaveable() const { return saveable; }
    QString music() const { return musicFolder->text().trimmed(); }
    QString cover() const;

Q_SIGNALS:
    void updated();

private Q_SLOTS:
    void configureFilenameScheme();
    void checkSaveable();
    void transcoderChanged();

private:
    FilenameSchemeDialog *schemeDlg;
    Device::Options origOpts;
    QString origMusicFolder;
    QString origCoverName;
    QString noCoverText;
    bool modified;
    bool saveable;
};

#endif
