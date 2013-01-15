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
#include <QtGui/QIcon>
#include "streamdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "streamsmodel.h"
#include "localize.h"
#include "icons.h"
#include "mpdconnection.h"
#include "buddylabel.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIconDialog>
#include <QtGui/QPushButton>
#endif

StreamDialog::StreamDialog(const QStringList &categories, const QStringList &genres, QWidget *parent, bool addToPlayQueue)
    : Dialog(parent)
    , urlHandlers(MPDConnection::self()->urlHandlers())
{
    QWidget *wid = new QWidget(this);
    QFormLayout *layout = new QFormLayout(wid);

    nameEntry = new LineEdit(wid);
    urlEntry = new LineEdit(wid);
    catCombo = new CompletionCombo(wid);
    catCombo->setEditable(true);
    genreCombo = new CompletionCombo(wid);
    statusText = new QLabel(this);

    QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(catCombo->sizePolicy().hasHeightForWidth());
    catCombo->setSizePolicy(sizePolicy);
    genreCombo->setSizePolicy(sizePolicy);
    urlEntry->setMinimumWidth(300);
    #ifdef ENABLE_KDE_SUPPORT
    iconButton=new QPushButton(this);
    iconButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setIcon(QString());
    #endif
    int row=0;

    if (addToPlayQueue) {
        saveButton=new OnOffButton(wid);
        saveButton->setChecked(false);
        layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Stream:"), wid, urlEntry));
        layout->setWidget(row++, QFormLayout::FieldRole, urlEntry);
        layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Save stream:"), wid, saveButton));
        layout->setWidget(row++, QFormLayout::FieldRole, saveButton);
        connect(saveButton, SIGNAL(toggled(bool)), nameEntry, SLOT(setEnabled(bool)));
        connect(saveButton, SIGNAL(toggled(bool)), catCombo, SLOT(setEnabled(bool)));
        connect(saveButton, SIGNAL(toggled(bool)), genreCombo, SLOT(setEnabled(bool)));
        nameEntry->setEnabled(false);
        catCombo->setEnabled(false);
        genreCombo->setEnabled(false);
        nameEntry->setEnabled(false);
        #ifdef ENABLE_KDE_SUPPORT
        connect(saveButton, SIGNAL(toggled(bool)), iconButton, SLOT(setEnabled(bool)));
        iconButton->setEnabled(false);
        #endif
    }
    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Name:"), wid, nameEntry));
    layout->setWidget(row++, QFormLayout::FieldRole, nameEntry);
    #ifdef ENABLE_KDE_SUPPORT
    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Icon:"), wid, iconButton));
    layout->setWidget(row++, QFormLayout::FieldRole, iconButton);
    #endif
    if (!addToPlayQueue) {
        layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Stream:"), wid, urlEntry));
        layout->setWidget(row++, QFormLayout::FieldRole, urlEntry);
    }
    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Category:"), wid, catCombo));
    layout->setWidget(row++, QFormLayout::FieldRole, catCombo);
    layout->setWidget(row, QFormLayout::LabelRole, new BuddyLabel(i18n("Genre:"), wid, genreCombo));
    layout->setWidget(row++, QFormLayout::FieldRole, genreCombo);
    layout->setWidget(row++, QFormLayout::SpanningRole, statusText);
    setCaption(i18n("Add Stream"));
    setMainWidget(wid);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    catCombo->clear();
    catCombo->insertItems(0, categories);
    genreCombo->clear();
    genreCombo->insertItems(0, genres);
    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(urlEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(catCombo, SIGNAL(editTextChanged(const QString &)), SLOT(changed()));
    connect(genreCombo, SIGNAL(editTextChanged(const QString &)), SLOT(changed()));
    #ifdef ENABLE_KDE_SUPPORT
    connect(iconButton, SIGNAL(clicked()), SLOT(setIcon()));
    #endif
    nameEntry->setFocus();
    resize(400, 100);
}

void StreamDialog::setEdit(const QString &cat, const QString &editName, const QString &editGenre, const QString &editIconName, const QString &editUrl)
{
    #ifdef ENABLE_KDE_SUPPORT
    prevIconName=iconName=editIconName;
    setIcon(prevIconName);
    #else
    Q_UNUSED(editIconName)
    #endif
    setCaption(i18n("Edit Stream"));
    enableButton(Ok, false);
    prevName=editName;
    prevUrl=editUrl;
    prevCat=cat;
    prevGenre=editGenre;
    nameEntry->setText(editName);
    urlEntry->setText(editUrl);
    catCombo->setEditText(cat);
    genreCombo->setEditText(editGenre);
}

void StreamDialog::changed()
{
    QString u=url();
    bool validProtocol=u.isEmpty() || urlHandlers.contains(QUrl(u).scheme()) || urlHandlers.contains(u);
    bool enableOk=false;

    if (saveButton && !saveButton->isChecked()) {
        enableOk=!u.isEmpty();
    } else {
        QString n=name();
        QString c=category();
        QString g=genre();
        enableOk=!n.isEmpty() && !u.isEmpty() && !c.isEmpty() && (n!=prevName || u!=prevUrl || c!=prevCat || g!=prevGenre
                                                                      #ifdef ENABLE_KDE_SUPPORT
                                                                      || icon()!=prevIconName
                                                                      #endif
                                                                      );

        statusText->setText(validProtocol ? QString() : i18n("<i>Invalid protocol</i>"));
    }
    enableOk=enableOk && validProtocol;
    enableButton(Ok, enableOk);
}

#ifdef ENABLE_KDE_SUPPORT
void StreamDialog::setIcon(const QString &icn)
{
    iconButton->setIcon(icn.isEmpty() ? Icons::streamIcon
                                      : icn.startsWith('/')
                                            ? QIcon(icn)
                                            : QIcon::fromTheme(icn));
}

void StreamDialog::setIcon()
{
    QString icon=KIconDialog::getIcon(KIconLoader::MainToolbar, KIconLoader::Any, false, 22, false, this);
    if (!icon.isEmpty()) {
        iconName=icon;
        setIcon(icon);
        changed();
    }
}
#endif
