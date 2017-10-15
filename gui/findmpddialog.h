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

#ifndef FINDMPDDIALOG_H
#define FINDMPDDIALOG_H

#include "avahidiscovery.h"
#include "ui_findmpddialog.h"

class FindMpdDialog : public QDialog, private Ui::FindMpdDialog
{
    Q_OBJECT

public:
    FindMpdDialog(QWidget *p);

Q_SIGNALS:
    void serverChosen(QString ip, QString port);

public Q_SLOTS:
    void addMpd(QString name, QString address, int port);
    void removeMpd(QString name);
    void okClicked();

private:
    QScopedPointer<AvahiDiscovery> avahi;
};


#endif // FINDMPDDIALOG_H
