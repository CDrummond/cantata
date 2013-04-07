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

#include <QFormLayout>
#include <QIcon>
#include "streamcategorydialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "streamsmodel.h"
#include "localize.h"
#include "buddylabel.h"
#include "icon.h"

StreamCategoryDialog::StreamCategoryDialog(const QStringList &categories, QWidget *parent)
    : Dialog(parent)
    , iconCombo(0)
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

    QMap<QString, QIcon> icons=StreamsModel::self()->icons();
    if (!icons.isEmpty()) {
        iconCombo=new QComboBox(this);
        iconCombo->addItem(i18n("No Icon"), QString());
        int size=Icon::stdSize(fontMetrics().height()*1.5);
        iconCombo->setIconSize(QSize(size,size));
        QMap<QString, QIcon>::ConstIterator it=icons.constBegin();
        QMap<QString, QIcon>::ConstIterator end=icons.constEnd();

        for (; it!=end; ++it) {
            if (!it.value().isNull()) {
                iconCombo->addItem(it.value(), QString(), it.key());
            }
        }

        layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Icon:"), wid, iconCombo));
        layout->setWidget(row++, QFormLayout::FieldRole, iconCombo);
        connect(iconCombo, SIGNAL(currentIndexChanged(int)), SLOT(changed()));
    }
    setCaption(i18n("Add Category"));
    setMainWidget(wid);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);

    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    nameEntry->setFocus();
    resize(400, 100);
}

void StreamCategoryDialog::setEdit(const QString &editName, const QString &editIconName)
{
    prevIconName=editIconName;
    if (iconCombo) {
        iconCombo->blockSignals(true);
        iconCombo->setCurrentIndex(0);
        for (int i=0; i<iconCombo->count(); ++i) {
            if (iconCombo->itemData(i)==editIconName) {
                iconCombo->setCurrentIndex(i);
                break;
            }
        }
        iconCombo->blockSignals(false);
    }
    setCaption(i18n("Edit Category"));
    enableButton(Ok, false);
    prevName=editName;
    nameEntry->blockSignals(true);
    nameEntry->setText(editName);
    nameEntry->blockSignals(false);
}

void StreamCategoryDialog::changed()
{
    QString n=name();
    bool enableOk=!n.isEmpty() && (n!=prevName || (iconCombo && icon()!=prevIconName) || !existingCategories.contains(n));
    enableButton(Ok, enableOk);
}
