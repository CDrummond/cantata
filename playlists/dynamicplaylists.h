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

#ifndef DYNAMIC_PLAYLISTS_H
#define DYNAMIC_PLAYLISTS_H

#include <QIcon>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "rulesplaylists.h"
#include "models/actionmodel.h"
#include "support/icon.h"

class QTimer;
class NetworkJob;

class DynamicPlaylists : public RulesPlaylists
{
    Q_OBJECT

public:
    enum Command {
        Unknown,
        Ping,
        List,
        Status,
        Save,
        Del,
        SetActive,
        Control
    };

    static Command toCommand(const QString &cmd);
    static QString toString(Command cmd);
    static void enableDebug();

    static DynamicPlaylists * self();

    DynamicPlaylists();
    ~DynamicPlaylists() override { }

    QString name() const override;
    QString title() const override;
    QString descr() const override;
    bool isDynamic() const override { return true; }
    QVariant data(const QModelIndex &index, int role) const override;
    bool isRemote() const override { return usingRemote; }
    bool saveRemote(const QString &string, const Entry &e) override;
    void del(const QString &name) override;
    void start(const QString &name);
    void stop(bool sendClear=false) override;
    void toggle(const QString &name);
    bool isRunning();
    void helperMessage(const QString &message) {  Q_UNUSED(message) checkHelper(); }
    Action * startAct() const { return startAction; }
    Action * stopAct() const { return stopAction; }
    void enableRemotePolling(bool e);

Q_SIGNALS:
    void running(bool status);
    void error(const QString &str);

    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void clear();
    void remoteMessage(const QStringList &args);

    // These are as the result of asynchronous HTTP calls
    void saved(bool s);
    void loadingList();
    void loadedList();

private Q_SLOTS:
    void checkHelper();
    void checkIfRemoteIsRunning();
    void updateRemoteStatus();
    void remoteResponse(QStringList msg);
    void remoteDynamicSupported(bool s);
    void parseStatus(QStringList response);

private:
    void pollRemoteHelper();
    int getPid() const;
    bool controlApp(bool isStart);
    bool sendCommand(Command cmd, const QStringList &args=QStringList());
    void parseRemote(const QStringList &response);

private:
    QTimer *localTimer;
    Action *startAction;
    Action *stopAction;

    // For remote dynamic servers...
    bool usingRemote;
    QTimer *remoteTimer;
    bool remotePollingEnabled;
    int statusTime;
    QString lastState;
    QString dynamicUrl;
    NetworkJob *currentJob;
    Command currentCommand;
    QString currentDelete;
    Entry currentSave;
};

#endif
