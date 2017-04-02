/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "models/actionmodel.h"
#include "support/icon.h"

class QTimer;
class NetworkJob;

class Dynamic : public ActionModel
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

    typedef QMap<QString, QString> Rule;
    struct Entry {
        Entry(const QString &n=QString()) : name(n), ratingFrom(0), ratingTo(0) { }
        bool operator==(const Entry &o) const { return name==o.name; }
        bool haveRating() const { return ratingFrom>=0 && ratingTo>0; }
        QString name;
        QList<Rule> rules;
        int ratingFrom;
        int ratingTo;
    };

    static Dynamic * self();

    static const QString constRuleKey;
    static const QString constArtistKey;
    static const QString constSimilarArtistsKey;
    static const QString constAlbumArtistKey;
    static const QString constComposerKey;
    static const QString constCommentKey;
    static const QString constAlbumKey;
    static const QString constTitleKey;
    static const QString constGenreKey;
    static const QString constDateKey;
    static const QString constRatingKey;
    static const QString constFileKey;
    static const QString constExactKey;
    static const QString constExcludeKey;
    static const QChar constRangeSep;

    Dynamic();
    virtual ~Dynamic() { }

    QString name() const;
    QString title() const;
    QString descr() const;
    const Icon & icon() const { return icn; }
    bool isRemote() const { return usingRemote; }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    bool hasChildren(const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Entry entry(const QString &e);
    Entry entry(int row) const { return row>=0 && row<entryList.count() ? entryList.at(row) : Entry(); }
    bool exists(const QString &e) { return entryList.end()!=find(e); }
    bool save(const Entry &e);
    void del(const QString &name);
    void start(const QString &name);
    void stop(bool sendClear=false);
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
    QList<Entry>::Iterator find(const QString &e);
    bool sendCommand(Command cmd, const QStringList &args=QStringList());
    void loadLocal();
    void parseRemote(const QStringList &response);
    void updateEntry(const Entry &e);

private:
    Icon icn;
    QTimer *localTimer;
    QList<Entry> entryList;
    QString currentEntry;
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
