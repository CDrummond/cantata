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

#include "onlinesettings.h"
#include "onlineservicesmodel.h"
#include "basicitemdelegate.h"
#include "icon.h"
#include "localize.h"
#include <QListWidget>

OnlineSettings::OnlineSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    providers->setItemDelegate(new BasicItemDelegate(providers));
    providers->setSortingEnabled(true);
    int iSize=Icon::stdSize(QApplication::fontMetrics().height()*1.25);
    providers->setIconSize(QSize(iSize, iSize));
}

void OnlineSettings::load()
{
    QList<OnlineServicesModel::Provider> provs=OnlineServicesModel::self()->getProviders();
    foreach (const OnlineServicesModel::Provider &prov, provs) {
        QListWidgetItem *item=new QListWidgetItem(prov.name, providers);
        item->setCheckState(prov.hidden ? Qt::Unchecked : Qt::Checked);
        item->setData(Qt::UserRole, prov.key);
        item->setIcon(prov.icon);
    }
}

void OnlineSettings::save()
{
    QSet<QString> disabled;
    for (int i=0; i<providers->count(); ++i) {
        QListWidgetItem *item=providers->item(i);
        if (Qt::Unchecked==item->checkState()) {
            QString id=item->data(Qt::UserRole).toString();
            if (OnlineServicesModel::self()->serviceIsBusy(id)) {
                item->setCheckState(Qt::Checked);
            } else {
                disabled.insert(id);
            }
        }
    }
    OnlineServicesModel::self()->setHiddenProviders(disabled);
}
