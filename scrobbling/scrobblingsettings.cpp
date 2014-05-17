/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "scrobblingsettings.h"
#include "scrobbler.h"
#include "support/localize.h"
#include "support/buddylabel.h"
#include "support/lineedit.h"
#include <QPushButton>

ScrobblingSettings::ScrobblingSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(loginButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(Scrobbler::self(), SIGNAL(authenticated(bool)), SLOT(showStatus(bool)));
    connect(Scrobbler::self(), SIGNAL(error(QString)), SLOT(showError(QString)));
    connect(user, SIGNAL(textChanged(QString)), SLOT(controlLoginButton()));
    connect(pass, SIGNAL(textChanged(QString)), SLOT(controlLoginButton()));
    connect(enableScrobbling, SIGNAL(toggled(bool)), Scrobbler::self(), SLOT(setEnabled(bool)));
    connect(Scrobbler::self(), SIGNAL(enabled(bool)), enableScrobbling, SLOT(setChecked(bool)));
    loginButton->setEnabled(false);
}

void ScrobblingSettings::load()
{
    showStatus(Scrobbler::self()->isAuthenticated());
    user->setText(Scrobbler::self()->user().trimmed());
    pass->setText(Scrobbler::self()->pass().trimmed());
    enableScrobbling->setChecked(Scrobbler::self()->isScrobblingEnabled());
    controlLoginButton();
}

void ScrobblingSettings::save()
{
    messageWidget->close();
    loginStatusLabel->setText(i18n("Authenticating..."));
    Scrobbler::self()->setDetails(user->text().trimmed(), pass->text().trimmed());
}

void ScrobblingSettings::showStatus(bool status)
{
    loginStatusLabel->setText(status ? i18n("Authenticated") : i18n("Not Authenticated"));
    if (status) {
        messageWidget->close();
    }
    enableScrobbling->setEnabled(status);
}

void ScrobblingSettings::showError(const QString &msg)
{
    messageWidget->setError(msg, true);
}

void ScrobblingSettings::controlLoginButton()
{
    loginButton->setEnabled(!user->text().trimmed().isEmpty() && !pass->text().trimmed().isEmpty());
}
