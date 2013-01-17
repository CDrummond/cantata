/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/****************************************************************************************
 * Copyright (c) 2010 Téo Mrnjavac <teo@kde.org>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef VALUESLIDER_H
#define VALUESLIDER_H

#include <QtGui/QLabel>
#include <QtGui/QSlider>
#include "encoders.h"

class ValueSlider : public QWidget
{
    Q_OBJECT
public:
    explicit ValueSlider(QWidget *parent=0);

    void setValues(const Encoders::Encoder &enc);
    void setValue(int value);
    int value() const { return slider->value(); }

Q_SIGNALS:
    void valueChanged(int value);

private Q_SLOTS:
    void onSliderChanged(int value);

private:
    QLabel *valueTypeLabel;
    QSlider *slider;
    QLabel *midLabel;
    QLabel *leftLabel;
    QLabel *rightLabel;
    int defaultSetting;
    QList<Encoders::Setting> settings;
};


#endif
