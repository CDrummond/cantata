/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "customactions.h"
#include "mainwindow.h"
#include "support/globalstatic.h"
#include "support/configuration.h"
#include "support/localize.h"
#include <QMenu>
#include <QProcess>

GLOBAL_STATIC(CustomActions, instance)

bool CustomActions::Command::operator<(const Command &o) const
{
    int c=name.localeAwareCompare(o.name);
    if (c<0) {
        return true;
    }
    if (c==0) {
        return cmd.localeAwareCompare(o.cmd)<0;
    }
    return false;
}

CustomActions::CustomActions()
    : Action(i18n("Custom Actions"), 0)
    , mainWindow(0)
{
    QMenu *m=new QMenu(0);
    setMenu(m);
    Configuration cfg(metaObject()->className());
    int count=cfg.get("count", 0);
    for (int i=0; i<count; ++i) {
        Command cmd(cfg.get(QString::number(i)+QLatin1String("_name"), QString()),
                    cfg.get(QString::number(i)+QLatin1String("_cmd"), QString()));
        if (!cmd.name.isEmpty() && !cmd.cmd.isEmpty()) {
            cmd.act=new Action(cmd.name, this);
            m->addAction(cmd.act);
            commands.append(cmd);
            connect(cmd.act, SIGNAL(triggered()), this, SLOT(doAction()));
        }
    }
    setVisible(!commands.isEmpty());
}

void CustomActions::set(QList<Command> cmds)
{
    qSort(cmds);
    bool diff=cmds.length()!=commands.length();

    if (!diff) {
        for (int i=0; i<cmds.length() && !diff; ++i) {
            if (commands[i]!=cmds[i]) {
                diff=true;
            }
        }
    }
    QMenu *m=menu();
    if (diff) {
        foreach (const Command &cmd, commands) {
            m->removeAction(cmd.act);
            disconnect(cmd.act, SIGNAL(triggered()), this, SLOT(doAction()));
            cmd.act->deleteLater();
        }
        commands.clear();

        foreach (const Command &cmd, cmds) {
            Command c(cmd);
            c.act=new Action(c.name, this);
            m->addAction(c.act);
            commands.append(c);
            connect(c.act, SIGNAL(triggered()), this, SLOT(doAction()));
        }

        Configuration cfg;
        cfg.removeGroup(metaObject()->className());
        if (!commands.isEmpty()) {
            cfg.beginGroup(metaObject()->className());
            cfg.set("count", commands.count());
            for (int i=0; i<commands.count(); ++i) {
                cfg.set(QString::number(i)+QLatin1String("_name"), commands[i].name);
                cfg.set(QString::number(i)+QLatin1String("_cmd"), commands[i].cmd);
            }
        }
    }

    setVisible(!commands.isEmpty());
}

void CustomActions::doAction()
{
    if (!mainWindow) {
        return;
    }
    Action *act=qobject_cast<Action *>(sender());
    if (!act) {
        return;
    }
    if (!MPDConnection::self()->getDetails().dirReadable) {
        return;
    }
    QString mpdDir=MPDConnection::self()->getDetails().dir;
    foreach (const Command &cmd, commands) {
        if (cmd.act==act) {
            QList<Song> songs=mainWindow->selectedSongs();
            if (songs.isEmpty()) {
                return;
            }
            QStringList items;
            if (cmd.cmd.contains("%d")) {
                QSet<QString> used;
                foreach (const Song &s, songs) {
                    if (Song::Playlist!=s.type) {
                        QString dir=Utils::getDir(s.file);
                        if (!used.contains(dir)) {
                            used.insert(dir);
                            items.append(mpdDir+dir);
                        }
                    }
                }
            } else {
                foreach (const Song &s, songs) {
                    if (Song::Playlist!=s.type) {
                        items.append(mpdDir+s.file);
                    }
                }
            }

            if (!items.isEmpty()) {
                QStringList parts=cmd.cmd.split(' ');
                bool added=false;
                QString cmd=parts.takeFirst();
                QStringList args;
                foreach (const QString &part, parts) {
                    if (part.startsWith('%')) {
                        args+=items;
                        added=true;
                    } else {
                        args+=part;
                    }
                }
                if (!added) {
                    args+=items;
                }
                QProcess::startDetached(cmd, args);
            }
            return;
        }
    }
}
