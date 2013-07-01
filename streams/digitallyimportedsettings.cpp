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

#include "digitallyimportedsettings.h"
#include "digitallyimported.h"
#include "localize.h"
#include "buddylabel.h"
#include "lineedit.h"
#include <QPushButton>
#include <QComboBox>

DigitallyImportedSettings::DigitallyImportedSettings(QWidget *parent)
    : Dialog(parent)
{
    setButtons(Ok|Cancel);
    setCaption(i18n("Digitally Imported Settings"));
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);

    audio->addItem(i18n("MP3 256k"), 0);
    audio->addItem(i18n("AAC 64k"), 1);
    audio->addItem(i18n("AAC 128k"), 2);

    connect(loginButton, SIGNAL(clicked()), this, SLOT(login()));
    connect(DigitallyImported::self(), SIGNAL(loginStatus(bool,QString)), SLOT(loginStatus(bool,QString)));

    adjustSize();
    int h=fontMetrics().height();
    resize(h*30, height());
}

void DigitallyImportedSettings::show()
{
    prevUser=DigitallyImported::self()->user();
    prevPass=DigitallyImported::self()->pass();
    wasLoggedIn=DigitallyImported::self()->loggedIn();
    prevAudioType=DigitallyImported::self()->audioType();

    if (DigitallyImported::self()->sessionExpiry().isValid()) {
        expiryLabel->setText(DigitallyImported::self()->sessionExpiry().toString(Qt::ISODate));
    } else {
        expiryLabel->setText(QString());
    }
    user->setText(prevUser);
    pass->setText(prevPass);

    for (int i=0; i<audio->count(); ++i) {
        if (audio->itemData(i).toInt()==DigitallyImported::self()->audioType()) {
            audio->setCurrentIndex(i);
            break;
        }
    }

    loginStatusLabel->setText(DigitallyImported::self()->statusString());
    if (QDialog::Accepted==Dialog::exec()) {
        QString u=user->text().trimmed();
        QString p=pass->text().trimmed();
        int at=audio->itemData(audio->currentIndex()).toInt();
        bool changed=false;
        bool needToLogin=false;
        if (u!=DigitallyImported::self()->user()) {
            DigitallyImported::self()->setUser(u);
            needToLogin=changed=true;
        }
        if (p!=DigitallyImported::self()->pass()) {
            DigitallyImported::self()->setPass(p);
            needToLogin=changed=true;
        }
        if (DigitallyImported::self()->audioType()!=at) {
            DigitallyImported::self()->setAudioType(at);
            changed=true;
        }
        if (needToLogin) {
            DigitallyImported::self()->login();
        }
        if (changed) {
            DigitallyImported::self()->save();
        }
    } else {
        DigitallyImported::self()->setUser(prevUser);
        DigitallyImported::self()->setPass(prevPass);
        DigitallyImported::self()->setAudioType(prevAudioType);
        if (wasLoggedIn) {
            DigitallyImported::self()->login();
        }
    }
}

void DigitallyImportedSettings::login()
{
    loginStatusLabel->setText(i18n("Logging in..."));
    DigitallyImported::self()->setUser(user->text().trimmed());
    DigitallyImported::self()->setPass(pass->text().trimmed());
    DigitallyImported::self()->login();
}

void DigitallyImportedSettings::loginStatus(bool, const QString &msg)
{
    loginStatusLabel->setText(msg);
    if (DigitallyImported::self()->sessionExpiry().isValid()) {
        expiryLabel->setText(DigitallyImported::self()->sessionExpiry().toString(Qt::ISODate));
    } else {
        expiryLabel->setText(QString());
    }
}
