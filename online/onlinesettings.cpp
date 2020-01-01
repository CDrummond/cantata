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

#include "onlinesettings.h"
#include "models/onlineservicesmodel.h"
#include "onlineservice.h"
#include "widgets/basicitemdelegate.h"
#include "support/icon.h"
#include <QListWidget>

enum Roles {
    KeyRole = Qt::UserRole,
    ConfigurableRole
};

OnlineSettings::OnlineSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    providers->setItemDelegate(new BasicItemDelegate(providers));
    providers->setSortingEnabled(true);
    int iSize=Icon::stdSize(QApplication::fontMetrics().height()*1.25);
    providers->setIconSize(QSize(iSize, iSize));    
    connect(providers, SIGNAL(currentRowChanged(int)), SLOT(currentProviderChanged(int)));
    connect(configureButton, SIGNAL(clicked()), this, SLOT(configure()));
    configureButton->setEnabled(false);
}

void OnlineSettings::load()
{
    QList<OnlineServicesModel::Provider> provs=OnlineServicesModel::self()->getProviders();
    for (const OnlineServicesModel::Provider &prov: provs) {
        QListWidgetItem *item=new QListWidgetItem(prov.name, providers);
        item->setCheckState(prov.hidden ? Qt::Unchecked : Qt::Checked);
        item->setData(KeyRole, prov.key);
        item->setData(ConfigurableRole, prov.configurable);
        item->setIcon(prov.icon);
    }
}

void OnlineSettings::save()
{
    QSet<QString> disabled;
    for (int i=0; i<providers->count(); ++i) {
        QListWidgetItem *item=providers->item(i);
        if (Qt::Unchecked==item->checkState()) {
            QString id=item->data(KeyRole).toString();
            if (OnlineServicesModel::self()->serviceIsBusy(id)) {
                item->setCheckState(Qt::Checked);
            } else {
                disabled.insert(id);
            }
        }
    }
    OnlineServicesModel::self()->setHiddenProviders(disabled);
}

void OnlineSettings::currentProviderChanged(int row)
{
    bool enableConfigure=false;

    if (row>=0) {
        QListWidgetItem *item=providers->item(row);
        enableConfigure=item->data(ConfigurableRole).toBool();
    }
    configureButton->setEnabled(enableConfigure);
}

void OnlineSettings::configure()
{
    int row=providers->currentRow();
    if (row<0) {
        return;
    }

    QListWidgetItem *item=providers->item(row);
    if (!item->data(ConfigurableRole).toBool()) {
        return;
    }

    OnlineService *srv=OnlineServicesModel::self()->service(item->data(KeyRole).toString());
    if (srv && srv->canConfigure()) {
        srv->configure(this);
    }
}

#include "moc_onlinesettings.cpp"
