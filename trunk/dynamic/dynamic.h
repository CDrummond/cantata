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

#ifndef DYNAMIC_H
#define DYNAMIC_H

#include <QIcon>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "actionmodel.h"

class QTimer;
class QNetworkReply;
class QUdpSocket;

class MulticastReceiver : public QObject
{
    Q_OBJECT
public:
    MulticastReceiver(QObject *parent, const QString &i, const QString &group, quint16 port);

Q_SIGNALS:
    void status(const QString &st);

private Q_SLOTS:
    void processMessages();

private:
    QUdpSocket *socket;
    QString id;
};

class Dynamic : public ActionModel
{
    Q_OBJECT

public:
    enum Command {
        Unknown,
        Id,
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

    typedef QMap<QString, QString> Rule;
    struct Entry {
        Entry(const QString &n=QString()) : name(n) { }
        bool operator==(const Entry &o) const { return name==o.name; }
        QString name;
        QList<Rule> rules;
    };

    static Dynamic * self();

    static const QString constRuleKey;
    static const QString constArtistKey;
    static const QString constSimilarArtistsKey;
    static const QString constAlbumArtistKey;
    static const QString constAlbumKey;
    static const QString constTitleKey;
    static const QString constGenreKey;
    static const QString constDateKey;
    static const QString constExactKey;
    static const QString constExcludeKey;

    Dynamic();
    virtual ~Dynamic() { stopReceiver(); }

    bool isRemote() const { return !dynamicUrl.isEmpty(); }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    bool hasChildren(const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Entry entry(const QString &e);
    bool exists(const QString &e) { return entryList.end()!=find(e); }
    bool save(const Entry &e);
    void del(const QString &name);
    void start(const QString &name);
    void stop();
    void toggle(const QString &name);
    bool isRunning();
    QString current() const { return currentEntry; }
    const QList<Entry> & entries() const { return entryList; }
    void helperMessage(const QString &message) {  Q_UNUSED(message) checkHelper(); }
    Action * startAct() const { return startAction; }
    Action * stopAct() const { return stopAction; }
    void enableRemotePolling(bool e);

Q_SIGNALS:
    void running(bool status);
    void remoteRunning(bool status);
    void error(const QString &str);

    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void clear();

    // These are as the result of asynchronous HTTP calls
    void saved(bool s);
    void loadingList();
    void loadedList();

public Q_SLOTS:
    void refreshList();

private Q_SLOTS:
    void checkHelper();
    void checkIfRemoteIsRunning();
    void updateRemoteStatus();
    void remoteJobFinished();
    void dynamicUrlChanged(const QString &url);
    void parseStatus(const QString &response);

private:
    void pollRemoteHelper();
    int getPid() const;
    bool controlApp(bool isStart);
    QList<Entry>::Iterator find(const QString &e);
    void sendCommand(Command cmd, const QStringList &args=QStringList());
    void loadLocal();
    void loadRemote();
    void parseRemote(const QString &response);
    void parseId(const QString &response);
    void checkResponse(const QString &response);
    void updateEntry(const Entry &e);
    void stopReceiver();
    void startReceiver(const QString &id, const QString &group, quint16 port);

private:
    QTimer *localTimer;
    QList<Entry> entryList;
    QString currentEntry;
    Action *startAction;
    Action *stopAction;

    // For remote dynamic servers...
    QTimer *remoteTimer;
    bool remotePollingEnabled;
    int statusTime;
    QString lastState;
    QString dynamicUrl;
    QNetworkReply *currentJob;
    Command currentCommand;
    QStringList currentArgs;
    Entry currentSave;
    MulticastReceiver *receiver;
};

#endif
