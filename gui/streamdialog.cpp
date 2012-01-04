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

#include <QtGui/QFormLayout>
#include <QtGui/QLabel>
#include <QtGui/QIcon>
#include "streamdialog.h"
#include "mainwindow.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#else
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>
#endif

StreamDialog::StreamDialog(const QStringList &categories, QWidget *parent)
#ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
#else
    : QDialog(parent)
#endif
{
    QWidget *wid = new QWidget(this);
    QFormLayout *layout = new QFormLayout(wid);

    nameEntry = new LineEdit(wid);
    urlEntry = new LineEdit(wid);
    catCombo = new QComboBox(wid);
    catCombo->setEditable(true);
    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(catCombo->sizePolicy().hasHeightForWidth());
    catCombo->setSizePolicy(sizePolicy);

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
    layout->setWidget(2, QFormLayout::LabelRole, new QLabel(i18n("Category:"), wid));
#else
    layout->setWidget(2, QFormLayout::LabelRole, new QLabel(tr("Category:"), wid));
#endif
    layout->setWidget(2, QFormLayout::FieldRole, catCombo);
#ifdef ENABLE_KDE_SUPPORT
    setMainWidget(wid);
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Add Stream"));
    enableButton(KDialog::Ok, false);
#else
    setWindowTitle(tr("Add Stream"));
    QBoxLayout *mainLayout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    mainLayout->addWidget(wid);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
#endif
    foreach (const QString &c, categories) {
        catCombo->insertItem(catCombo->count(), c);
    }
    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(urlEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(catCombo, SIGNAL(editTextChanged(const QString &)), SLOT(changed()));
    nameEntry->setFocus();
}

void StreamDialog::setEdit(const QString &cat, const QString &editName, const QString &editUrl)
{
#ifdef ENABLE_KDE_SUPPORT
    setCaption(i18n("Edit Stream"));
    enableButton(KDialog::Ok, false);
#else
    setWindowTitle(tr("Edit Stream"));
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
#endif
    prevName=editName;
    prevUrl=editUrl;
    prevCat=cat;
    nameEntry->setText(editName);
    urlEntry->setText(editUrl);
    catCombo->setEditText(cat);
}

void StreamDialog::changed()
{
    QString n=name();
    QString u=url();
    QString c=category();
    bool enableOk=!n.isEmpty() && !u.isEmpty() && !c.isEmpty() && (n!=prevName || u!=prevUrl || c!=prevCat);
#ifdef ENABLE_KDE_SUPPORT
    enableButton(KDialog::Ok, enableOk);
#else
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enableOk);
#endif
}
