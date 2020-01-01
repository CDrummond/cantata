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

#ifndef CUSTOM_ACTIONS_H
#define CUSTOM_ACTIONS_H

#include "support/action.h"

class MainWindow;

class CustomActions : public Action
{
    Q_OBJECT
public:
    struct Command
    {
        Command(const QString &n=QString(), const QString &c=QString(), Action *a=nullptr) : name(n.trimmed()), cmd(c.trimmed()), act(a) { }
        bool operator<(const Command &o) const;
        bool operator==(const Command &o) const { return name==o.name && cmd==o.cmd; }
        bool operator!=(const Command &o) const { return !(*this==o); }
        QString name;
        QString cmd;
        Action *act;
    };

    static void enableDebug();
    static CustomActions * self();
    CustomActions();

    void set(QList<Command> cmds);
    const QList<Command> & commandList() const { return commands; }
    void setMainWindow(MainWindow *mw) { mainWindow=mw; }

private Q_SLOTS:
    void doAction();

private:
    MainWindow *mainWindow;
    QList<Command> commands;
};

#endif
