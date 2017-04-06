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

#include "dynamic.h"
#include "config.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "widgets/icons.h"
#include "models/roles.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "support/localize.h"
#include "gui/plurals.h"
#include "support/actioncollection.h"
#include "support/globalstatic.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QIcon>
#include <QUrl>
#include <QNetworkProxy>
#include <signal.h>
#include <stdio.h>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void Dynamic::enableDebug()
{
    debugEnabled=true;
}

static const QString constDir=QLatin1String("dynamic");
static const QString constExtension=QLatin1String(".rules");
static const QString constActiveRules=QLatin1String("rules");
static const QString constLockFile=QLatin1String("lock");

static const QString constPingCmd=QLatin1String("ping");
static const QString constListCmd=QLatin1String("list");
static const QString constStatusCmd=QLatin1String("status");
static const QString constSaveCmd=QLatin1String("save");
static const QString constDeleteCmd=QLatin1String("delete");
static const QString constSetActiveCmd=QLatin1String("setActive");
static const QString constControlCmd=QLatin1String("control");

static QString remoteError(const QStringList &status)
{
    if (!status.isEmpty()) {
        switch (status.at(0).toInt()) {
        case 1:  return i18n("Empty filename.");
        case 2:  return i18n("Invalid filename. (%1)", status.length()<2 ? QString() : status.at(2));
        case 3:  return i18n("Failed to save %1.", status.length()<2 ? QString() : status.at(2));
        case 4:  return i18n("Failed to delete rules file. (%1)", status.length()<2 ? QString() : status.at(2));
        case 5:  return i18n("Invalid command. (%1)", status.length()<2 ? QString() : status.at(2));
        case 6:  return i18n("Could not remove active rules link.");
        case 7:  return i18n("Active rules is not a link.");
        case 8:  return i18n("Could not create active rules link.");
        case 9:  return i18n("Rules file, %1, does not exist.", status.length()<2 ? QString() : status.at(2));
        case 10: return i18n("Incorrect arguments supplied.");
        case 11: return i18n("Unknown method called.");
        }
    }
    return i18n("Unknown error");
}

Dynamic::Command Dynamic::toCommand(const QString &cmd)
{
    if (constListCmd==cmd) {
        return List;
    }
    if (constStatusCmd==cmd) {
        return Status;
    }
    if (constSaveCmd==cmd) {
        return Save;
    }
    if (constDeleteCmd==cmd) {
        return Del;
    }
    if (constSetActiveCmd==cmd) {
        return SetActive;
    }
    if (constControlCmd==cmd) {
        return Control;
    }
    return Unknown;
}

QString Dynamic::toString(Command cmd)
{
    switch (cmd) {
    case Unknown:   return QString();
    case Ping:      return constPingCmd;
    case List:      return constListCmd;
    case Status:    return constStatusCmd;
    case Save:      return constSaveCmd;
    case Del:       return constDeleteCmd;
    case SetActive: return constSetActiveCmd;
    case Control:   return constControlCmd;
    }
    return QString();
}

GLOBAL_STATIC(Dynamic, instance)

const QString Dynamic::constRuleKey=QLatin1String("Rule");
const QString Dynamic::constArtistKey=QLatin1String("Artist");
const QString Dynamic::constSimilarArtistsKey=QLatin1String("SimilarArtists");
const QString Dynamic::constAlbumArtistKey=QLatin1String("AlbumArtist");
const QString Dynamic::constComposerKey=QLatin1String("Composer");
const QString Dynamic::constCommentKey=QLatin1String("Comment");
const QString Dynamic::constAlbumKey=QLatin1String("Album");
const QString Dynamic::constTitleKey=QLatin1String("Title");
const QString Dynamic::constGenreKey=QLatin1String("Genre");
const QString Dynamic::constDateKey=QLatin1String("Date");
const QString Dynamic::constRatingKey=QLatin1String("Rating");
const QString Dynamic::constDurationKey=QLatin1String("Duration");
const QString Dynamic::constFileKey=QLatin1String("File");
const QString Dynamic::constExactKey=QLatin1String("Exact");
const QString Dynamic::constExcludeKey=QLatin1String("Exclude");
const QChar Dynamic::constRangeSep=QLatin1Char('-');

const QChar constKeyValSep=QLatin1Char(':');
const QString constOk=QLatin1String("0");
const QString constFilename=QLatin1String("FILENAME:");

Dynamic::Dynamic()
    : localTimer(0)
    , usingRemote(false)
    , remoteTimer(0)
    , remotePollingEnabled(false)
    , statusTime(0)
    , currentJob(0)
    , currentCommand(Unknown)
{
    icn=Icons::self()->dynamicRuleIcon;
    loadLocal();
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(MPDConnection::self(), SIGNAL(dynamicSupport(bool)), this, SLOT(remoteDynamicSupported(bool)));
    connect(this, SIGNAL(remoteMessage(QStringList)), MPDConnection::self(), SLOT(sendDynamicMessage(QStringList)));
    connect(MPDConnection::self(), SIGNAL(dynamicResponse(QStringList)), this, SLOT(remoteResponse(QStringList)));
    QTimer::singleShot(500, this, SLOT(checkHelper()));
    startAction = ActionCollection::get()->createAction("startdynamic", i18n("Start Dynamic Playlist"), Icons::self()->replacePlayQueueIcon);
    stopAction = ActionCollection::get()->createAction("stopdynamic", i18n("Stop Dynamic Mode"), Icons::self()->stopDynamicIcon);
}

QString Dynamic::name() const
{
    return QLatin1String("dynamic");
}

QString Dynamic::title() const
{
    return i18n("Dynamic Playlists");
}

QString Dynamic::descr() const
{
    return i18n("Dynamically generated playlists");
}

QVariant Dynamic::headerData(int, Qt::Orientation, int) const
{
    return QVariant();
}

int Dynamic::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : entryList.count();
}

bool Dynamic::hasChildren(const QModelIndex &parent) const
{
    return !parent.isValid();
}

QModelIndex Dynamic::parent(const QModelIndex &) const
{
    return QModelIndex();
}

QModelIndex Dynamic::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || !hasIndex(row, column, parent) || row>=entryList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

#define IS_ACTIVE(E) !currentEntry.isEmpty() && (E)==currentEntry && (!isRemote() || QLatin1String("IDLE")!=lastState)

QVariant Dynamic::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return title();
        case Cantata::Role_SubText:
            return descr();
        case Qt::DecorationRole:
            return icon();
        }
        return QVariant();
    }

    if (index.parent().isValid() || index.row()>=entryList.count()) {
        return QVariant();
    }

    switch (role) {
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
    case Qt::DisplayRole:
        return entryList.at(index.row()).name;
    case Qt::DecorationRole:
        return IS_ACTIVE(entryList.at(index.row()).name) ? Icons::self()->replacePlayQueueIcon : Icons::self()->dynamicListIcon;
    case Cantata::Role_SubText: {
        const Entry &e=entryList.at(index.row());
        return Plurals::rules(e.rules.count())+(e.haveRating() ? i18n(" - Rating: %1..%2", (double)e.ratingFrom/Song::Rating_Step, (double)e.ratingTo/Song::Rating_Step) : QString());
    }
    case Cantata::Role_Actions: {
        QVariant v;
        v.setValue<QList<Action *> >(QList<Action *>() << (IS_ACTIVE(entryList.at(index.row()).name) ? stopAction : startAction));
        return v;
    }
    default:
        return QVariant();
    }
}

Qt::ItemFlags Dynamic::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }
    return Qt::NoItemFlags;
}

Dynamic::Entry Dynamic::entry(const QString &e)
{
    if (!e.isEmpty()) {
        QList<Entry>::Iterator it=find(e);
        if (it!=entryList.end()) {
            return *it;
        }
    }

    return Entry();
}

bool Dynamic::save(const Entry &e)
{
    if (e.name.isEmpty()) {
        return false;
    }

    QString string;
    QTextStream str(&string);
    if (e.ratingFrom!=0 || e.ratingTo!=0) {
        str << constRatingKey << constKeyValSep << e.ratingFrom << constRangeSep << e.ratingTo<< '\n';
    }
    if (e.minDuration!=0 || e.maxDuration!=0) {
        str << constDurationKey << constKeyValSep << e.minDuration << constRangeSep << e.maxDuration<< '\n';
    }
    foreach (const Rule &rule, e.rules) {
        if (!rule.isEmpty()) {
            str << constRuleKey << '\n';
            Rule::ConstIterator it(rule.constBegin());
            Rule::ConstIterator end(rule.constEnd());
            for (; it!=end; ++it) {
                str << it.key() << constKeyValSep << it.value() << '\n';
            }
        }
    }

    if (isRemote()) {
        if (sendCommand(Save, QStringList() << e.name << string)) {
            currentSave=e;
            return true;
        }
        return false;
    }
qWarning() << string;
    QFile f(Utils::dataDir(constDir, true)+e.name+constExtension);
    if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << string;
        updateEntry(e);
        return true;
    }
    return false;
}

void Dynamic::updateEntry(const Entry &e)
{
    QList<Dynamic::Entry>::Iterator it=find(e.name);
    if (it!=entryList.end()) {
        entryList.replace(it-entryList.begin(), e);
        QModelIndex idx=index(it-entryList.begin(), 0, QModelIndex());
        emit dataChanged(idx, idx);
    } else {
        beginInsertRows(QModelIndex(), entryList.count(), entryList.count());
        entryList.append(e);
        endInsertRows();
    }
}

void Dynamic::del(const QString &name)
{
    if (isRemote()) {
        if (sendCommand(Del, QStringList() << name)) {
            currentDelete=name;
        }
        return;
    }

    QList<Dynamic::Entry>::Iterator it=find(name);
    if (it==entryList.end()) {
        return;
    }
    QString fName(Utils::dataDir(constDir, false)+name+constExtension);
    bool isCurrent=currentEntry==name;

    if (!QFile::exists(fName) || QFile::remove(fName)) {
        if (isCurrent) {
            stop();
        }
        beginRemoveRows(QModelIndex(), it-entryList.begin(), it-entryList.begin());
        entryList.erase(it);
        endRemoveRows();
        return;
    }
}

void Dynamic::start(const QString &name)
{
    if (isRemote()) {
        sendCommand(SetActive, QStringList() << name << "1");
        return;
    }

    if (Utils::findExe("perl").isEmpty()) {
        emit error(i18n("You need to install \"perl\" on your system in order for Cantata's dynamic mode to function."));
        return;
    }

    QString fName(Utils::dataDir(constDir, false)+name+constExtension);

    if (!QFile::exists(fName)) {
        emit error(i18n("Failed to locate rules file - %1", fName));
        return;
    }

    QString rules(Utils::cacheDir(constDir, true)+constActiveRules);

    QFile::remove(rules);
    if (QFile::exists(rules)) {
        emit error(i18n("Failed to remove previous rules file - %1", rules));
        return;
    }

    if (!QFile::link(fName, rules)) {
        emit error(i18n("Failed to install rules file - %1 -> %2", fName, rules));
        return;
    }

    int i=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
    QModelIndex idx=index(i, 0, QModelIndex());

    currentEntry=name;

    if (idx.isValid()) {
        emit dataChanged(idx, idx);
    }

    i=entryList.indexOf(currentEntry);
    idx=index(i, 0, QModelIndex());
    if (idx.isValid()) {
        emit dataChanged(idx, idx);
    }

    if (isRunning()) {
        emit clear();
        return;
    }
    if (controlApp(true)) {
        emit running(isRunning());
        emit clear();
        return;
    }
}

void Dynamic::stop(bool sendClear)
{
    if (isRemote()) {
        if (sendClear) {
            sendCommand(Control, QStringList() << "stop" << "1");
        } else {
            sendCommand(Control, QStringList() << "stop");
        }
        return;
    }

    #if !defined Q_OS_WIN
    int i=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
    QModelIndex idx=index(i, 0, QModelIndex());
    int pid=getPid();

    if (!pid) {
        if (sendClear) {
            emit clear();
        }
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return;
    }

    if (0!=::kill(pid, 0)) {
        if (sendClear) {
            emit clear();
        }
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return;
    }

    if (controlApp(false)) {
        if (sendClear) {
            emit clear();
        }
        currentEntry=QString();
        emit running(isRunning());
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return;
    }
    #endif
}

void Dynamic::toggle(const QString &name)
{
    if(name==currentEntry) {
        stop();
    } else {
        start(name);
    }
}

bool Dynamic::isRunning()
{
    #if defined Q_OS_WIN
    return false;
    #else
    int pid=getPid();
    return pid ? 0==::kill(pid, 0) : false;
    #endif
}

void Dynamic::enableRemotePolling(bool e)
{
    remotePollingEnabled=e;
    if (remoteTimer && remoteTimer->isActive()) {
        if (remotePollingEnabled) {
            checkIfRemoteIsRunning();
        }
        remoteTimer->start(remotePollingEnabled ? 5000 : 30000);
    }
}

int Dynamic::getPid() const
{
    QFile pidFile(Utils::cacheDir(constDir, false)+constLockFile);

    if (pidFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream str(&pidFile);
        int pid=0;
        str >> pid;
        return pid;
    }

    return 0;
}

bool Dynamic::controlApp(bool isStart)
{
    QString cmd=CANTATA_SYS_SCRIPTS_DIR+QLatin1String("cantata-dynamic");
    QProcess process;

    if (isStart) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        MPDConnectionDetails details=MPDConnection::self()->getDetails();
        env.insert("MPD_HOST", details.password.isEmpty() ? details.hostname : (details.password+'@'+details.hostname));
        env.insert("MPD_PORT", QString::number(details.port));
        process.setProcessEnvironment(env);
    }
    process.start(cmd, QStringList() << QLatin1String(isStart ? "start" : "stop"), QIODevice::WriteOnly);

    if (!localTimer) {
        localTimer=new QTimer(this);
        connect(localTimer, SIGNAL(timeout()), SLOT(checkHelper()));
    }
    bool rv=process.waitForFinished(1000);
    localTimer->start(1000);
    return rv;
}

QList<Dynamic::Entry>::Iterator Dynamic::find(const QString &e)
{
    QList<Dynamic::Entry>::Iterator it(entryList.begin());
    QList<Dynamic::Entry>::Iterator end(entryList.end());

    for (; it!=end; ++it) {
        if ((*it).name==e) {
            break;
        }
    }
    return it;
}

void Dynamic::loadLocal()
{
    beginResetModel();
    entryList.clear();
    currentEntry=QString();

    // Load all current enttries...
    QString dirName=Utils::dataDir(constDir);
    QDir d(dirName);
    if (d.exists()) {
        QStringList rulesFiles=d.entryList(QStringList() << QChar('*')+constExtension);
        foreach (const QString &rf, rulesFiles) {
            QFile f(dirName+rf);
            if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QStringList keys=QStringList() << constArtistKey << constSimilarArtistsKey << constAlbumArtistKey << constDateKey
                                               << constExactKey << constAlbumKey << constTitleKey << constGenreKey << constFileKey << constExcludeKey;

                Entry e;
                e.name=rf.left(rf.length()-constExtension.length());
                Rule r;
                QTextStream in(&f);
                in.setCodec("UTF-8");
                QStringList lines = in.readAll().split('\n', QString::SkipEmptyParts);
                foreach (const QString &line, lines) {
                    QString str=line.trimmed();

                    if (str.isEmpty() || str.startsWith('#')) {
                        continue;
                    }

                    if (str==constRuleKey) {
                        if (!r.isEmpty()) {
                            e.rules.append(r);
                            r.clear();
                        }
                    } else if (str.startsWith(constRatingKey+constKeyValSep)) {
                        QStringList vals=str.mid(constRatingKey.length()+1).split(constRangeSep);
                        if (2==vals.count()) {
                            e.ratingFrom=vals.at(0).toUInt();
                            e.ratingTo=vals.at(1).toUInt();
                        }
                    } else if (str.startsWith(constDurationKey+constKeyValSep)) {
                        QStringList vals=str.mid(constDurationKey.length()+1).split(constRangeSep);
                        if (2==vals.count()) {
                            e.minDuration=vals.at(0).toUInt();
                            e.maxDuration=vals.at(1).toUInt();
                        }
                    } else {
                        foreach (const QString &k, keys) {
                            if (str.startsWith(k+constKeyValSep)) {
                                r.insert(k, str.mid(k.length()+1));
                            }
                        }
                    }
                }
                if (!r.isEmpty()) {
                    e.rules.append(r);
                    r.clear();
                }
                entryList.append(e);
            }
        }
    }
    endResetModel();
}

void Dynamic::parseRemote(const QStringList &response)
{
    DBUG << response;
    beginResetModel();
    entryList.clear();
    currentEntry=QString();
    QStringList keys=QStringList() << constArtistKey << constSimilarArtistsKey << constAlbumArtistKey << constDateKey
                                   << constExactKey << constAlbumKey << constTitleKey << constGenreKey << constFileKey << constExcludeKey;
    Entry e;
    Rule r;

    foreach (const QString &part, response) {
        QStringList lines=part.split('\n', QString::SkipEmptyParts);
        foreach (const QString &s, lines) {
            QString str=s.trimmed();
            if (str.isEmpty() || str.startsWith('#')) {
                continue;
            }
            if (str.startsWith(constFilename)) {
                if (!e.name.isEmpty()) {
                    if (!r.isEmpty()) {
                        e.rules.append(r);
                    }
                    entryList.append(e);
                }
                e.name=str.mid(9, str.length()-15); // Remove extension...
                e.ratingFrom=e.ratingTo=0;
                e.rules.clear();
                r.clear();
            } else if (str==constRuleKey) {
                if (!r.isEmpty()) {
                    e.rules.append(r);
                    r.clear();
                }
            } else if (str.startsWith(constRatingKey+constKeyValSep)) {
                QStringList vals=str.mid(constRatingKey.length()+1).split(constRangeSep);
                if (2==vals.count()) {
                    e.ratingFrom=vals.at(0).toUInt();
                    e.ratingTo=vals.at(1).toUInt();
                }
            } else if (str.startsWith(constDurationKey+constKeyValSep)) {
                QStringList vals=str.mid(constDurationKey.length()+1).split(constRangeSep);
                if (2==vals.count()) {
                    e.minDuration=vals.at(0).toUInt();
                    e.maxDuration=vals.at(1).toUInt();
                }
            } else {
                foreach (const QString &k, keys) {
                    if (str.startsWith(k+constKeyValSep)) {
                        r.insert(k, str.mid(k.length()+1));
                    }
                }
            }
        }
    }

    if (!e.name.isEmpty()) {
        if (!r.isEmpty()) {
            e.rules.append(r);
        }
        entryList.append(e);
    }
    endResetModel();
}

void Dynamic::parseStatus(QStringList response)
{
    DBUG << response;
    if (response.isEmpty()) {
        return;
    }
    QString state=response.takeAt(0);
    QString prevEntry=currentEntry;
    int st=statusTime;

    if (!response.isEmpty()) {
        st=response.takeAt(0).toInt();
    }
    if (!response.isEmpty()) {
        currentEntry=response.takeAt(0);
    }
    bool stateChanged=lastState!=state;
    bool noSongs=QLatin1String("NO_SONGS")==state;
    bool terminated=false;
    DBUG << "lastState" << lastState << "state" << state << "statusTime(before)" << statusTime << "statusTime(now)" << st;

    if (stateChanged) {
        lastState=state;
        if (noSongs) {
            bool sendError=!currentEntry.isEmpty();
            emit running(true);
            if (sendError) {
                emit error(QLatin1String("NO_SONGS"));
            }
            //sendCommand(Control, QStringList() << "stop");
        } else if (QLatin1String("HAVE_SONGS")==state || QLatin1String("STARTING")==state) {
            emit running(true);
        } else if (QLatin1String("IDLE")==state) {
            currentEntry.clear();
            emit running(false);
        } else if (QLatin1String("TERMINATED")==state) {
            terminated=true;
            currentEntry.clear();
            emit running(false);
            emit error(i18n("Dynamizer has been terminated."));
            pollRemoteHelper();
            currentEntry=QString();
        }
    }

    if (prevEntry!=currentEntry) {
        int prev=prevEntry.isEmpty() ? -1 : entryList.indexOf(prevEntry);
        int cur=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
        if (-1!=prev) {
            QModelIndex idx=index(prev, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
        if (-1!=cur) {
            QModelIndex idx=index(cur, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
    } else if (stateChanged) {
        int row=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
        if (-1!=row) {
            QModelIndex idx=index(row, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
    }

    if (st>statusTime && !terminated) {
        statusTime=st;
        sendCommand(List, QStringList() << "1");
    }
}

bool Dynamic::sendCommand(Command cmd, const QStringList &args)
{
    DBUG << toString(cmd) << args;
    if (Ping==cmd) {
        emit remoteMessage(QStringList() << toString(cmd));
        return true;
    }

    if (Status==currentCommand) {
        if (cmd==Status) {
            return true;
        }
        currentCommand=Unknown;
    }
    if (Status!=cmd && (Save==currentCommand || Del==currentCommand)) {
        emit error(i18n("Awaiting response for previous command. (%1)", Save==cmd ? i18n("Saving rule") : i18n("Deleting rule")));
        return false;
    }
    currentCommand=cmd;

    emit remoteMessage(QStringList() << toString(cmd) << args);
    if (List==cmd) {
        emit loadingList();
    }
    return true;
}

void Dynamic::checkHelper()
{
    if (isRemote()) {
        return;
    }

    if (!isRunning()) {
        emit running(false);
        int i=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
        currentEntry=QString();
        if (i>-1) {
            QModelIndex idx=index(i, 0, QModelIndex());
            emit dataChanged(idx, idx);
        }
        if (localTimer) {
            localTimer->stop();
        }
    } else {
        if (localTimer && localTimer->isActive()) {
            static const int constAppCheck=15*1000;
            if (localTimer->interval()<constAppCheck) {
                localTimer->start(constAppCheck);
            }
        } else { // No timer => app startup!
            // Attempt to read current name...
            QFileInfo inf(Utils::cacheDir(constDir, false)+constActiveRules);

            if (inf.exists() && inf.isSymLink()) {
                QString link=inf.readLink();
                if (!link.isEmpty()) {
                    QString fname=QFileInfo(link).fileName();
                    if (fname.endsWith(constExtension)) {
                        currentEntry=fname.left(fname.length()-constExtension.length());
                    }
                }
            }
            emit running(true);
        }
    }
}

void Dynamic::pollRemoteHelper()
{
    if (!remoteTimer) {
        remoteTimer=new QTimer(this);
        connect(remoteTimer, SIGNAL(timeout()), SLOT(checkIfRemoteIsRunning()));
    }
    beginResetModel();
    entryList.clear();
    currentEntry=QString();
    endResetModel();
    remoteTimer->start(remotePollingEnabled ? 5000 : 30000);
}

void Dynamic::checkIfRemoteIsRunning()
{
    if (isRemote()) {
        if (remotePollingEnabled) {
            sendCommand(Ping);
        }
    } else if (remoteTimer) {
        remoteTimer->stop();
    }
}

void Dynamic::updateRemoteStatus()
{
    if (isRemote()) {
        sendCommand(Status);
    }
}

void Dynamic::remoteResponse(QStringList msg)
{
    DBUG << msg;
    if (msg.isEmpty()) {
        return;
    }

    QString cmd=msg.takeAt(0);
    Command msgCmd=toCommand(cmd);

    switch (msgCmd) {
    case List:
        parseRemote(msg);
        emit loadedList();
        break;
    case Status:
        parseStatus(msg);
        break;
    case Save:
        if (!msg.isEmpty() && msg.at(0)==constOk) {
            updateEntry(currentSave);
            emit saved(true);
        } else {
            emit error(i18n("Failed to save %1. (%2)", currentSave.name, remoteError(msg)));
            emit saved(false);
        }
        currentSave.name=QString();
        currentSave.rules.clear();
        break;
    case Del:
        if (!msg.isEmpty() && msg.at(0)==constOk) {
            QList<Dynamic::Entry>::Iterator it=find(currentDelete);
            if (it!=entryList.end()) {
                beginRemoveRows(QModelIndex(), it-entryList.begin(), it-entryList.begin());
                entryList.erase(it);
                endRemoveRows();
            } else {
                emit error(i18n("Failed to delete rules file. (%1)", remoteError(msg)));
            }
        } else {
            emit error(i18n("Failed to delete rules file. (%1)", remoteError(msg)));
            emit saved(false);
        }
        currentDelete.clear();
        break;
    case Control:
        if (msg.isEmpty() || msg.at(0)!=constOk) {
            emit error(i18n("Failed to control dynamizer state. (%1)", remoteError(msg)));
        }
        break;
    case SetActive:
        if (!msg.isEmpty() && msg.at(0)==constOk) {
            QTimer::singleShot(1000, this, SLOT(updateRemoteStatus()));
        } else {
            emit error(i18n("Failed to set the current dynamic rules. (%1)", remoteError(msg)));
        }
        break;
    default:
        break;
    }

    if (msgCmd==currentCommand) {
        currentCommand=Unknown;
    }
}

void Dynamic::remoteDynamicSupported(bool s)
{
    if (usingRemote==s) {
        return;
    }
    // Stop any local dynamizer before switching to remote...
    if (!isRemote()) {
        stop();
    }
    usingRemote=s;
    if (s) {
        if (localTimer) {
            localTimer->stop();
        }
        pollRemoteHelper();
        sendCommand(List);
        sendCommand(Status);
    } else {
        if (remoteTimer) {
            remoteTimer->stop();
        }
        loadLocal();
//        #ifndef Q_OS_WIN
//        emit error(i18n("Communication with the remote dynamizer has been lost, reverting to local mode."));
//        #endif
    }
}
