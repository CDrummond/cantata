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

#include "scrobblingsettings.h"
#include "scrobbler.h"
#include "support/buddylabel.h"
#include "support/lineedit.h"
#include <QPushButton>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

ScrobblingSettings::ScrobblingSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    connect(loginButton, SIGNAL(clicked()), this, SLOT(save()));
    connect(Scrobbler::self(), SIGNAL(authenticated(bool)), SLOT(showStatus(bool)));
    connect(Scrobbler::self(), SIGNAL(error(QString)), SLOT(showError(QString)));
    connect(user, SIGNAL(textChanged(QString)), SLOT(controlLoginButton()));
    connect(pass, SIGNAL(textChanged(QString)), SLOT(controlLoginButton()));
//    connect(enableScrobbling, SIGNAL(toggled(bool)), Scrobbler::self(), SLOT(setEnabled(bool)));
//    connect(showLove, SIGNAL(toggled(bool)), Scrobbler::self(), SLOT(setLoveEnabled(bool)));
    connect(Scrobbler::self(), SIGNAL(enabled(bool)), enableScrobbling, SLOT(setChecked(bool)));
    connect(scrobbler, SIGNAL(currentIndexChanged(int)), this, SLOT(scrobblerChanged()));
    loginButton->setEnabled(false);

    QMap<QString, QString> scrobblers=Scrobbler::self()->availableScrobblers();
    QStringList keys=scrobblers.keys();
    keys.sort();
    QString firstMpdClient;
    for (const QString &k: keys) {
        bool viaMpd=Scrobbler::viaMpd(scrobblers[k]);
        if (viaMpd) {
            scrobbler->addItem(tr("%1 (via MPD)", "scrobbler name (via MPD)").arg(k), k);
        } else {
            scrobbler->addItem(k);
        }
        if (viaMpd && firstMpdClient.isEmpty()) {
            firstMpdClient=k;
        }
    }
    if (scrobbler->count()>1) {
        scrobblerName->setVisible(false);
    } else {
        scrobblerName->setText(scrobbler->itemText(0));
        scrobbler->setVisible(false);
        scrobblerLabel->setBuddy(nullptr);
    }
    scrobblerName->setVisible(scrobbler->count()<2);
    if (firstMpdClient.isEmpty()) {
        REMOVE(noteLabel)
    } else {
        noteLabel->setText(tr("If you use a scrobbler which is marked as '(via MPD)' (such as %1), "
                                "then you will need to have this already started and running. "
                                "Cantata can only 'Love' tracks via this, and cannot enable/disable scrobbling.").arg(firstMpdClient));
    }

    #ifdef Q_OS_MAC
    expandingSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    #endif
}

void ScrobblingSettings::load()
{
    showStatus(Scrobbler::self()->isAuthenticated());
    user->setText(Scrobbler::self()->user().trimmed());
    pass->setText(Scrobbler::self()->pass().trimmed());
    QString s=Scrobbler::self()->activeScrobbler();
    for (int i=0; i<scrobbler->count(); ++i) {
        if (scrobbler->itemText(i)==s || scrobbler->itemData(i).toString()==s) {
            scrobbler->setCurrentIndex(i);
            break;
        }
    }
    enableScrobbling->setChecked(Scrobbler::self()->isEnabled());
    showLove->setChecked(Scrobbler::self()->isLoveEnabled());
    controlLoginButton();
    scrobblerChanged();
}

void ScrobblingSettings::save()
{
    bool isLogin=sender()==loginButton;
    messageWidget->close();
    QString u=user->text().trimmed();
    QString p=pass->text().trimmed();

    QString sc=scrobbler->itemData(scrobbler->currentIndex()).toString();
    if (sc.isEmpty()) {
        loginStatusLabel->setText(tr("Authenticating..."));
        sc=scrobbler->currentText();
    }

    Scrobbler::self()->setEnabled(enableScrobbling->isChecked());
    Scrobbler::self()->setLoveEnabled(showLove->isChecked());

    // We dont save password, so this /might/ be empty! Therefore, dont disconnect just
    // because user clicked OK/Apply...
    if (isLogin || sc!=Scrobbler::self()->activeScrobbler() || u!=Scrobbler::self()->user() ||
        (!Scrobbler::self()->pass().isEmpty() && p!=Scrobbler::self()->pass())) {
        Scrobbler::self()->setDetails(sc, u, pass->text().trimmed());
    }
}

void ScrobblingSettings::showStatus(bool status)
{
    loginStatusLabel->setText(status ? tr("Authenticated") : tr("Not Authenticated"));
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
    loginButton->setEnabled(user->isEnabled() && !user->text().trimmed().isEmpty() && !pass->text().trimmed().isEmpty());
}

void ScrobblingSettings::scrobblerChanged()
{
    bool viaMpd=!scrobbler->itemData(scrobbler->currentIndex()).toString().isEmpty();
    user->setEnabled(!viaMpd);
    userLabel->setEnabled(!viaMpd);
    pass->setEnabled(!viaMpd);
    passLabel->setEnabled(!viaMpd);
    loginStatusLabel->setEnabled(!viaMpd);
    statusLabel->setEnabled(!viaMpd);
    enableScrobbling->setEnabled(!viaMpd);
    if (viaMpd) {
        enableScrobbling->setChecked(false);
    }
    controlLoginButton();
}

#include "moc_scrobblingsettings.cpp"
