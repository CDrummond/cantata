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

#include <QtGui/QFormLayout>
#include <QtGui/QIcon>
#include "streamcategorydialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "streamsmodel.h"
#include "localize.h"
#include "icons.h"
#include "buddylabel.h"
//#ifdef ENABLE_KDE_SUPPORT
//#include <KDE/KIconDialog>
//#include <QtGui/QPushButton>
//#endif

StreamCategoryDialog::StreamCategoryDialog(const QStringList &categories, QWidget *parent)
    : Dialog(parent)
{
    existingCategories=categories.toSet();
    QWidget *wid = new QWidget(this);
    QFormLayout *layout = new QFormLayout(wid);

    nameEntry = new LineEdit(wid);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    int row=0;

    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Name:"), wid, nameEntry));
    layout->setWidget(row++, QFormLayout::FieldRole, nameEntry);
//    #ifdef ENABLE_KDE_SUPPORT
//    iconButton=new QPushButton(this);
//    iconButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
//    setIcon(QString());
//    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Icon:"), wid, iconButton));
//    layout->setWidget(row++, QFormLayout::FieldRole, iconButton);
//    #endif
    setCaption(i18n("Add Category"));
    setMainWidget(wid);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);

    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
//    #ifdef ENABLE_KDE_SUPPORT
//    connect(iconButton, SIGNAL(clicked()), SLOT(setIcon()));
//    #endif
    nameEntry->setFocus();
    resize(400, 100);
}

void StreamCategoryDialog::setEdit(const QString &editName, const QString &editIconName)
{
//    #ifdef ENABLE_KDE_SUPPORT
//    prevIconName=iconName=editIconName;
//    setIcon(prevIconName);
//    #else
    Q_UNUSED(editIconName)
//    #endif
    setCaption(i18n("Edit Category"));
    enableButton(Ok, false);
    prevName=editName;
    nameEntry->setText(editName);
}

void StreamCategoryDialog::changed()
{
    QString n=name();
    bool enableOk=!n.isEmpty() && (n!=prevName) && !existingCategories.contains(n);

//    #ifdef ENABLE_KDE_SUPPORT
//    enableOk=enableOk || icon()!=prevIconName;
//    #endif
    enableButton(Ok, enableOk);
}

//#ifdef ENABLE_KDE_SUPPORT
//void StreamCategoryDialog::setIcon(const QString &icn)
//{
//    iconButton->setIcon(icn.isEmpty() ? Icons::streamIcon
//                                      : icn.startsWith('/')
//                                            ? QIcon(icn)
//                                            : QIcon::fromTheme(icn));
//}

//void StreamCategoryDialog::setIcon()
//{
//    QString icon=KIconDialog::getIcon(KIconLoader::MainToolbar, KIconLoader::Any, false, 22, false, this);
//    if (!icon.isEmpty()) {
//        iconName=icon;
//        setIcon(icon);
//        changed();
//    }
//}
//#endif
