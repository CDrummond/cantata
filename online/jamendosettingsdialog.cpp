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

#include "jamendosettingsdialog.h"
#include "support/buddylabel.h"
#include <QFormLayout>

JamendoSettingsDialog::JamendoSettingsDialog(QWidget *parent)
    : Dialog(parent)
{
    setButtons(Ok|Cancel);
    setCaption(tr("Jamendo Settings"));
    QWidget *mw=new QWidget(this);
    QFormLayout *layout=new QFormLayout(mw);
    fmt=new QComboBox(mw);
    fmt->insertItem(0, tr("MP3"));
    fmt->insertItem(1, tr("Ogg"));
    layout->setWidget(0, QFormLayout::LabelRole, new BuddyLabel(tr("Streaming format:"), mw, fmt));
    layout->setWidget(0, QFormLayout::FieldRole, fmt);
    layout->setMargin(0);
    setMainWidget(mw);
}

bool JamendoSettingsDialog::run(bool mp3)
{
    fmt->setCurrentIndex(mp3 ? 0 : 1);
    return QDialog::Accepted==Dialog::exec();
}

#include "moc_jamendosettingsdialog.cpp"
