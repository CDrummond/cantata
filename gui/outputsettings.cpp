/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "outputsettings.h"
#include "mpdconnection.h"
#include <QtGui/QListWidget>

OutputSettings::OutputSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutpus(const QList<Output> &)));
};

void OutputSettings::load()
{
    MPDConnection::self()->outputs();
}

void OutputSettings::save()
{
    for (int i=0; i<view->count(); ++i) {
        QListWidgetItem *item=view->item(i);

        if (Qt::Checked==item->checkState()) {
            MPDConnection::self()->enableOutput(item->data(Qt::UserRole).toInt());
        } else {
            MPDConnection::self()->disableOutput(item->data(Qt::UserRole).toInt());
        }
    }
}

void OutputSettings::updateOutpus(const QList<Output> &outputs)
{
    view->clear();
    foreach(const Output &output, outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, view);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
    }
}
