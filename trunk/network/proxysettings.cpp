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
#include <QtCore/QSettings>

ProxySettings::ProxySettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

ProxySettings::~ProxySettings()
{
}

void ProxySettings::load()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::constSettingsGroup);

    int mode=s.value("mode", NetworkProxyFactory::Mode_System).toInt();
    proxySystem->setChecked(NetworkProxyFactory::Mode_System==mode);
    proxyDirect->setChecked(NetworkProxyFactory::Mode_Direct==mode);
    proxyManual->setChecked(NetworkProxyFactory::Mode_Manual==mode);
    proxyType->setCurrentIndex(QNetworkProxy::HttpProxy==s.value("type", QNetworkProxy::HttpProxy).toInt() ? 0 : 1);
    proxyHost->setText(s.value("hostname").toString());
    proxyPort->setValue(s.value("port", 8080).toInt());
    proxyAuth->setChecked(s.value("use_authentication", false).toBool());
    proxyUsername->setText(s.value("username").toString());
    proxyPassword->setText(s.value("password").toString());
    s.endGroup();
}

void ProxySettings::save()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::constSettingsGroup);

    s.setValue("mode", proxySystem->isChecked()
                            ? NetworkProxyFactory::Mode_System
                            : proxyDirect->isChecked()
                                ? NetworkProxyFactory::Mode_Direct
                                : NetworkProxyFactory::Mode_Manual);
    s.setValue("type", 0==proxyType->currentIndex() ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
    s.setValue("hostname", proxyHost->text());
    s.setValue("port", proxyPort->value());
    s.setValue("use_authentication", proxyAuth->isChecked());
    s.setValue("username", proxyUsername->text());
    s.setValue("password", proxyPassword->text());
    s.endGroup();
    NetworkProxyFactory::self()->reloadSettings();
}
