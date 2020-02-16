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

#include "dynamicplaylists.h"
#include "config.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "widgets/icons.h"
#include "models/roles.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "support/actioncollection.h"
#include "support/globalstatic.h"
#include "support/monoicon.h"
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
void DynamicPlaylists::enableDebug()
{
    debugEnabled=true;
}

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
        case 1:  return QObject::tr("Empty filename.");
        case 2:  return QObject::tr("Invalid filename. (%1)").arg(status.length()<2 ? QString() : status.at(2));
        case 3:  return QObject::tr("Failed to save %1.").arg(status.length()<2 ? QString() : status.at(2));
        case 4:  return QObject::tr("Failed to delete rules file. (%1)").arg(status.length()<2 ? QString() : status.at(2));
        case 5:  return QObject::tr("Invalid command. (%1)").arg(status.length()<2 ? QString() : status.at(2));
        case 6:  return QObject::tr("Could not remove active rules link.");
        case 7:  return QObject::tr("Active rules is not a link.");
        case 8:  return QObject::tr("Could not create active rules link.");
        case 9:  return QObject::tr("Rules file, %1, does not exist.").arg(status.length()<2 ? QString() : status.at(2));
        case 10: return QObject::tr("Incorrect arguments supplied.");
        case 11: return QObject::tr("Unknown method called.");
        }
    }
    return QObject::tr("Unknown error");
}

DynamicPlaylists::Command DynamicPlaylists::toCommand(const QString &cmd)
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

QString DynamicPlaylists::toString(Command cmd)
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

GLOBAL_STATIC(DynamicPlaylists, instance)

const QString constOk=QLatin1String("0");
const QString constFilename=QLatin1String("FILENAME:");

DynamicPlaylists::DynamicPlaylists()
    : RulesPlaylists(FontAwesome::random, "dynamic")
    , localTimer(nullptr)
    , usingRemote(false)
    , remoteTimer(nullptr)
    , remotePollingEnabled(false)
    , statusTime(0)
    , currentJob(nullptr)
    , currentCommand(Unknown)
{
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(MPDConnection::self(), SIGNAL(dynamicSupport(bool)), this, SLOT(remoteDynamicSupported(bool)));
    connect(this, SIGNAL(remoteMessage(QStringList)), MPDConnection::self(), SLOT(sendDynamicMessage(QStringList)));
    connect(MPDConnection::self(), SIGNAL(dynamicResponse(QStringList)), this, SLOT(remoteResponse(QStringList)));
    QTimer::singleShot(500, this, SLOT(checkHelper()));
    startAction = ActionCollection::get()->createAction("startdynamic", tr("Start Dynamic Playlist"), Icons::self()->replacePlayQueueIcon);
    stopAction = ActionCollection::get()->createAction("stopdynamic", tr("Stop Dynamic Mode"), Icons::self()->stopDynamicIcon);
}

QString DynamicPlaylists::name() const
{
    return QLatin1String("dynamic");
}

QString DynamicPlaylists::title() const
{
    return tr("Dynamic Playlists");
}

QString DynamicPlaylists::descr() const
{
    return tr("Dynamically generated playlists");
}

#define IS_ACTIVE(E) !currentEntry.isEmpty() && (E)==currentEntry && (!isRemote() || QLatin1String("IDLE")!=lastState)

QVariant DynamicPlaylists::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return RulesPlaylists::data(index, role);
    }

    if (index.parent().isValid() || index.row()>=entryList.count()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DecorationRole:
        return IS_ACTIVE(entryList.at(index.row()).name) ? Icons::self()->replacePlayQueueIcon : icn;
    case Cantata::Role_Actions: {
        QVariant v;
        v.setValue<QList<Action *> >(QList<Action *>() << (IS_ACTIVE(entryList.at(index.row()).name) ? stopAction : startAction));
        return v;
    }
    default:
        return RulesPlaylists::data(index, role);
    }
}

bool DynamicPlaylists::saveRemote(const QString &string, const Entry &e)
{
    if (sendCommand(Save, QStringList() << e.name << string)) {
        currentSave=e;
        return true;
    }
    return false;
}

void DynamicPlaylists::del(const QString &name)
{
    if (isRemote()) {
        if (sendCommand(Del, QStringList() << name)) {
            currentDelete=name;
        }
    } else {
        RulesPlaylists::del(name);
    }
}

void DynamicPlaylists::start(const QString &name)
{
    if (isRemote()) {
        sendCommand(SetActive, QStringList() << name << "1");
        return;
    }

    if (Utils::findExe("perl").isEmpty()) {
        emit error(tr("You need to install \"perl\" on your system in order for Cantata's dynamic mode to function."));
        return;
    }

    QString fName(Utils::dataDir(rulesDir, false)+name+constExtension);

    if (!QFile::exists(fName)) {
        emit error(tr("Failed to locate rules file - %1").arg(fName));
        return;
    }

    QString rules(Utils::cacheDir(rulesDir, true)+constActiveRules);

    QFile::remove(rules);
    if (QFile::exists(rules)) {
        emit error(tr("Failed to remove previous rules file - %1").arg(rules));
        return;
    }

    if (!QFile::link(fName, rules)) {
        emit error(tr("Failed to install rules file - %1 -> %2").arg(fName).arg(rules));
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

void DynamicPlaylists::stop(bool sendClear)
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

void DynamicPlaylists::toggle(const QString &name)
{
    if(name==currentEntry) {
        stop();
    } else {
        start(name);
    }
}

bool DynamicPlaylists::isRunning()
{
    #if defined Q_OS_WIN
    return false;
    #else
    int pid=getPid();
    return pid ? 0==::kill(pid, 0) : false;
    #endif
}

void DynamicPlaylists::enableRemotePolling(bool e)
{
    remotePollingEnabled=e;
    if (remoteTimer && remoteTimer->isActive()) {
        if (remotePollingEnabled) {
            checkIfRemoteIsRunning();
        }
        remoteTimer->start(remotePollingEnabled ? 5000 : 30000);
    }
}

int DynamicPlaylists::getPid() const
{
    QFile pidFile(Utils::cacheDir(rulesDir, false)+constLockFile);

    if (pidFile.open(QIODevice::ReadOnly|QIODevice::Text)) {
        QTextStream str(&pidFile);
        int pid=0;
        str >> pid;
        return pid;
    }

    return 0;
}

bool DynamicPlaylists::controlApp(bool isStart)
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
    if (isStart && rv) {
        int exitCode = process.exitCode();
        DBUG << exitCode;
        if (0!=exitCode) {
            emit error(tr("Could not start dynamic helper. Please check all <a href=\"https://github.com/CDrummond/cantata/wiki/Dynamic-Palylists-Helper\">dependencies</a> are installed."));
        }
    }
    localTimer->start(1000);
    return rv;
}

void DynamicPlaylists::parseRemote(const QStringList &response)
{
    DBUG << response;
    beginResetModel();
    entryList.clear();
    currentEntry=QString();
    QStringList keys=QStringList() << constArtistKey << constSimilarArtistsKey << constAlbumArtistKey << constDateKey
                                   << constExactKey << constAlbumKey << constTitleKey << constGenreKey << constFileKey << constExcludeKey;
    Entry e;
    Rule r;

    for (const QString &part: response) {
        QStringList lines=part.split('\n', QString::SkipEmptyParts);
        for (const QString &s: lines) {
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
                for (const QString &k: keys) {
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

void DynamicPlaylists::parseStatus(QStringList response)
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
            emit error(tr("Dynamizer has been terminated."));
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

bool DynamicPlaylists::sendCommand(Command cmd, const QStringList &args)
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
        emit error(tr("Awaiting response for previous command. (%1)").arg(Save==cmd ? tr("Saving rule") : tr("Deleting rule")));
        return false;
    }
    currentCommand=cmd;

    emit remoteMessage(QStringList() << toString(cmd) << args);
    if (List==cmd) {
        emit loadingList();
    }
    return true;
}

void DynamicPlaylists::checkHelper()
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
            QFileInfo inf(Utils::cacheDir(rulesDir, false)+constActiveRules);

            if (inf.exists() && inf.isSymLink()) {
                QString link=inf.symLinkTarget();
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

void DynamicPlaylists::pollRemoteHelper()
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

void DynamicPlaylists::checkIfRemoteIsRunning()
{
    if (isRemote()) {
        if (remotePollingEnabled) {
            sendCommand(Ping);
        }
    } else if (remoteTimer) {
        remoteTimer->stop();
    }
}

void DynamicPlaylists::updateRemoteStatus()
{
    if (isRemote()) {
        sendCommand(Status);
    }
}

void DynamicPlaylists::remoteResponse(QStringList msg)
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
            emit error(tr("Failed to save %1. (%2)").arg(currentSave.name).arg(remoteError(msg)));
            emit saved(false);
        }
        currentSave.name=QString();
        currentSave.rules.clear();
        break;
    case Del:
        if (!msg.isEmpty() && msg.at(0)==constOk) {
            QList<DynamicPlaylists::Entry>::Iterator it=find(currentDelete);
            if (it!=entryList.end()) {
                beginRemoveRows(QModelIndex(), it-entryList.begin(), it-entryList.begin());
                entryList.erase(it);
                endRemoveRows();
            } else {
                emit error(tr("Failed to delete rules file. (%1)").arg(remoteError(msg)));
            }
        } else {
            emit error(tr("Failed to delete rules file. (%1)").arg(remoteError(msg)));
            emit saved(false);
        }
        currentDelete.clear();
        break;
    case Control:
        if (msg.isEmpty() || msg.at(0)!=constOk) {
            emit error(tr("Failed to control dynamizer state. (%1)").arg(remoteError(msg)));
        }
        break;
    case SetActive:
        if (!msg.isEmpty() && msg.at(0)==constOk) {
            QTimer::singleShot(1000, this, SLOT(updateRemoteStatus()));
        } else {
            emit error(tr("Failed to set the current dynamic rules. (%1)").arg(remoteError(msg)));
        }
        break;
    default:
        break;
    }

    if (msgCmd==currentCommand) {
        currentCommand=Unknown;
    }
}

void DynamicPlaylists::remoteDynamicSupported(bool s)
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
//        emit error(tr("Communication with the remote dynamizer has been lost, reverting to local mode."));
//        #endif
    }
}

#include "moc_dynamicplaylists.cpp"
