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

#include "httpserversettings.h"
#include "settings.h"
#include "localize.h"
#include "httpserver.h"
#include <QTimer>

HttpServerSettings::HttpServerSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    httpPort->setSpecialValueText(i18n("Dynamic"));
    updateStatus();
}

void HttpServerSettings::load()
{
    int port=Settings::self()->httpPort();
    enableHttp->setChecked(Settings::self()->enableHttp());
    alwaysUseHttp->setChecked(Settings::self()->alwaysUseHttp());
    httpPort->setValue(port<1024 ? 1023 : port);
    httpAddress->setText(Settings::self()->httpAddress());
}

void HttpServerSettings::save()
{
    Settings::self()->saveEnableHttp(enableHttp->isChecked());
    Settings::self()->saveAlwaysUseHttp(alwaysUseHttp->isChecked());
    Settings::self()->saveHttpPort(httpPort->value());
    Settings::self()->saveHttpAddress(httpAddress->text());
    QTimer::singleShot(250, this, SLOT(updateStatus()));
}

void HttpServerSettings::updateStatus()
{
    if (HttpServer::self()->isAlive()) {
        status->setText(HttpServer::self()->address());
    } else {
        status->setText(i18n("Inactive"));
    }
}
