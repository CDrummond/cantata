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

#include "proxysettings.h"
#include "networkproxyfactory.h"
#include "localize.h"
#include <QSettings>

ProxySettings::ProxySettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    proxyMode->addItem(i18n("No proxy"), (int)NetworkProxyFactory::Mode_Direct);
    proxyMode->addItem(i18n("Use the system proxy settings"), (int)NetworkProxyFactory::Mode_System);
    proxyMode->addItem(i18n("Manual proxy configuration"), (int)NetworkProxyFactory::Mode_Manual);
    connect(proxyMode, SIGNAL(currentIndexChanged(int)), SLOT(toggleMode()));
}

ProxySettings::~ProxySettings()
{
}

void ProxySettings::load()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::constSettingsGroup);

    int mode=s.value("mode", NetworkProxyFactory::Mode_System).toInt();
    for (int i=0; i<proxyMode->count(); ++i) {
        if (proxyMode->itemData(i).toInt()==mode) {
            proxyMode->setCurrentIndex(i);
            break;
        }
    }

    proxyType->setCurrentIndex(QNetworkProxy::HttpProxy==s.value("type", QNetworkProxy::HttpProxy).toInt() ? 0 : 1);
    proxyHost->setText(s.value("hostname").toString());
    proxyPort->setValue(s.value("port", 8080).toInt());
    proxyUsername->setText(s.value("username").toString());
    proxyPassword->setText(s.value("password").toString());
    s.endGroup();
    toggleMode();
}

void ProxySettings::save()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::constSettingsGroup);

    s.setValue("mode", proxyMode->itemData(proxyMode->currentIndex()).toInt());
    s.setValue("type", 0==proxyType->currentIndex() ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
    s.setValue("hostname", proxyHost->text());
    s.setValue("port", proxyPort->value());
    s.setValue("username", proxyUsername->text());
    s.setValue("password", proxyPassword->text());
    s.endGroup();
    NetworkProxyFactory::self()->reloadSettings();
}

void ProxySettings::toggleMode()
{
    bool showManual=NetworkProxyFactory::Mode_Manual==proxyMode->itemData(proxyMode->currentIndex()).toInt();
    proxyType->setVisible(showManual);
    proxyTypeLabel->setVisible(showManual);
    proxyHost->setVisible(showManual);
    proxyHostLabel->setVisible(showManual);
    proxyPort->setVisible(showManual);
    proxyPortLabel->setVisible(showManual);
    proxyUsername->setVisible(showManual);
    proxyUsernameLabel->setVisible(showManual);
    proxyPassword->setVisible(showManual);
    proxyPasswordLabel->setVisible(showManual);
}
