/*
 *Copyright (C) <2017>  Alex B
 *
 *This program is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 3 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "findmpddialog.h"
#include <QPushButton>

FindMpdDialog::FindMpdDialog(QWidget *p)
    : QDialog(p)
    , avahi(new AvahiDiscovery())
{
    setupUi(this);

    tableWidget->setColumnCount(3);
    tableWidget->setHorizontalHeaderLabels({tr("Name"), tr("Address"), tr("Port")});
    tableWidget->resizeColumnToContents(0);
    tableWidget->resizeColumnToContents(1);

    QObject::connect(avahi.data(), &AvahiDiscovery::mpdFound, this, &FindMpdDialog::addMpd);
    QObject::connect(avahi.data(), &AvahiDiscovery::mpdRemoved, this, &FindMpdDialog::removeMpd);
    QObject::connect(this, &QDialog::accepted, this, &FindMpdDialog::okClicked);

    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    QObject::connect(tableWidget, &QTableWidget::itemSelectionChanged, this, [this](){ buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true); });
}

void FindMpdDialog::addMpd(QString name, QString address, int port)
{
    int rowCount = tableWidget->model()->rowCount();
    tableWidget->insertRow(rowCount);

    tableWidget->setItem(rowCount, 0, new QTableWidgetItem(name));
    tableWidget->setItem(rowCount, 1, new QTableWidgetItem(address));
    tableWidget->setItem(rowCount, 2, new QTableWidgetItem( QString::number(port) ));

    tableWidget->resizeColumnToContents(0);
    tableWidget->resizeColumnToContents(1);
}

void FindMpdDialog::removeMpd(QString name)
{
    for(int row = 0; row <= tableWidget->rowCount(); row++) {
        auto nameItem = tableWidget->item(row, 0);

        if (nameItem->text() == name) {
            tableWidget->removeRow(row);
            return;
        }
    }
}

void FindMpdDialog::okClicked()
{
    QItemSelectionModel *selection = tableWidget->selectionModel();

    if (selection->hasSelection()) {
        QModelIndexList indexList = selection->selectedRows();
        QModelIndex addressIndex = selection->selectedRows(1).first();
        QModelIndex portIndex = selection->selectedRows(2).first();

        emit serverChosen(tableWidget->item(addressIndex.row(), addressIndex.column())->text(), tableWidget->item(portIndex.row(), portIndex.column())->text());
    }
}

#include "moc_findmpddialog.cpp"
