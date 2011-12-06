/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtGui/QFormLayout>
#include <QtGui/QLabel>
#include <QtGui/QIcon>
#include "streamdialog.h"
#include "mainwindow.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

StreamDialog::StreamDialog(QWidget *parent)
#ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
#else
    : QDialog(parent)
#endif
{
#ifdef ENABLE_KDE_SUPPORT
    QWidget *wid = new QWidget(this);
#else
    QWidget *wid = this;
#endif
    QFormLayout *layout=new QFormLayout(wid);

    nameEntry=new LineEdit(wid);
    urlEntry=new LineEdit(wid);
#ifdef ENABLE_KDE_SUPPORT
    layout->setWidget(0, QFormLayout::LabelRole, new QLabel(i18n("Name:"), wid));
#else
    layout->setWidget(0, QFormLayout::LabelRole, new QLabel(tr("Name:"), wid));
#endif
    layout->setWidget(0, QFormLayout::FieldRole, nameEntry);
#ifdef ENABLE_KDE_SUPPORT
    layout->setWidget(1, QFormLayout::LabelRole, new QLabel(i18n("Stream:"), wid));
#else
    layout->setWidget(1, QFormLayout::LabelRole, new QLabel(tr("Stream:"), wid));
#endif
    layout->setWidget(1, QFormLayout::FieldRole, urlEntry);
    urlEntry->setMinimumWidth(300);

#ifdef ENABLE_KDE_SUPPORT
    setMainWidget(wid);
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Add Stream"));
#else
    setWindowTitle(tr("Add Stream"));
    // TODO: Buttons!!!!
#endif
}

