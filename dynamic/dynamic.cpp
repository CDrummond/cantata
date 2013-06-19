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

#include "dynamic.h"
#include "config.h"
#include "utils.h"
#include "mpdconnection.h"
#include "icons.h"
#include "itemview.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "localize.h"
#include "actioncollection.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(Dynamic, instance)
#endif
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QIcon>
#include <QUrl>
#include <QUdpSocket>
#include <QNetworkProxy>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <signal.h>

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

static const QString constIdCmd=QLatin1String("id");
static const QString constListCmd=QLatin1String("list");
static const QString constStatusCmd=QLatin1String("status");
static const QString constSaveCmd=QLatin1String("save");
static const QString constDeleteCmd=QLatin1String("delete");
static const QString constSetActiveCmd=QLatin1String("setActive");
static const QString constControlCmd=QLatin1String("control");
static const QString constStatusTime=QLatin1String("statusTime");
static const QString constStatusTimeResp=QLatin1String("TIME:");
static const QString constStatusStateResp=QLatin1String("STATE:");
static const QString constStatusRuleResp=QLatin1String("RULES:");
static const QString constUpdateRequiredResp=QLatin1String("UPDATE_REQUIRED");

Dynamic::Command Dynamic::toCommand(const QString &cmd)
{
    if (constIdCmd==cmd) {
        return Id;
    }
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
    case Id:        return constIdCmd;
    case List:      return constListCmd;
    case Status:    return constStatusCmd;
    case Save:      return constSaveCmd;
    case Del:       return constDeleteCmd;
    case SetActive: return constSetActiveCmd;
    case Control:   return constControlCmd;
    }
    return QString();
}

Dynamic * Dynamic::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Dynamic *instance=0;
    if(!instance) {
        instance=new Dynamic;
    }
    return instance;
    #endif
}

const QString Dynamic::constRuleKey=QLatin1String("Rule");
const QString Dynamic::constArtistKey=QLatin1String("Artist");
const QString Dynamic::constSimilarArtistsKey=QLatin1String("SimilarArtists");
const QString Dynamic::constAlbumArtistKey=QLatin1String("AlbumArtist");
const QString Dynamic::constAlbumKey=QLatin1String("Album");
const QString Dynamic::constTitleKey=QLatin1String("Title");
const QString Dynamic::constGenreKey=QLatin1String("Genre");
const QString Dynamic::constDateKey=QLatin1String("Date");
const QString Dynamic::constExactKey=QLatin1String("Exact");
const QString Dynamic::constExcludeKey=QLatin1String("Exclude");

static const char * constMulticastMsgHeader="{CANTATA/";
const QString constStatusMsg(QLatin1String("STATUS:"));

MulticastReceiver::MulticastReceiver(QObject *parent, const QString &i, const QString &group, quint16 port)
    : QObject(parent)
    , id(i)
{
    socket = new QUdpSocket(this);
    socket->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    #if QT_VERSION < 0x050000
    socket->bind(QHostAddress::Any, port, QUdpSocket::ShareAddress);
    #else
    socket->bind(QHostAddress::AnyIPv4, port, QAbstractSocket::ShareAddress);
    #endif
    socket->joinMulticastGroup(QHostAddress(group));
    connect(socket, SIGNAL(readyRead()), this, SLOT(processMessages()));
}

void MulticastReceiver::processMessages()
{
    static int headerLen=strlen(constMulticastMsgHeader);

    while (socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket->pendingDatagramSize());
        socket->readDatagram(datagram.data(), datagram.size());
        if (datagram.length()>headerLen && datagram.startsWith(constMulticastMsgHeader)) {
            QString header(constMulticastMsgHeader+id+"}");
            QString message=QString::fromUtf8(&(datagram.constData()[header.length()]));
            DBUG << message;
            if (message.startsWith(constStatusMsg)) {
                emit status(message.mid(constStatusMsg.length()));
            }
        }
    }
}

Dynamic::Dynamic()
    : localTimer(0)
    , remoteTimer(0)
    , statusTime(1)
    , currentJob(0)
    , currentCommand(Unknown)
    , receiver(0)
{
    loadLocal();
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(MPDConnection::self(), SIGNAL(dynamicUrl(const QString &)), this, SLOT(dynamicUrlChanged(const QString &)));
//    connect(MPDConnection::self(), SIGNAL(statusUpdated(const MPDStatusValues &)), this, SLOT(updateRemoteStatus()));
    QTimer::singleShot(500, this, SLOT(checkHelper()));
    startAction = ActionCollection::get()->createAction("startdynamic", i18n("Start Dynamic Playlist"), "media-playback-start");
    stopAction = ActionCollection::get()->createAction("stopdynamic", i18n("Stop Dynamic Mode"), "process-stop");
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
    if (!index.isValid() || index.parent().isValid() || index.row()>=entryList.count()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return entryList.at(index.row()).name;
    case Qt::DecorationRole:
        return IS_ACTIVE(entryList.at(index.row()).name) ? QIcon::fromTheme("media-playback-start") : Icons::self()->dynamicRuleIcon;
    case ItemView::Role_SubText: {
        #ifdef ENABLE_KDE_SUPPORT
        return i18np("1 Rule", "%1 Rules", entryList.at(index.row()).rules.count());
        #else
        return QTP_RULES_STR(entryList.at(index.row()).rules.count());
        #endif
    }
    case ItemView::Role_Actions: {
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
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    else
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
    foreach (const Rule &rule, e.rules) {
        if (!rule.isEmpty()) {
            str << constRuleKey << '\n';
            Rule::ConstIterator it(rule.constBegin());
            Rule::ConstIterator end(rule.constEnd());
            for (; it!=end; ++it) {
                str << it.key() << ':' << it.value() << '\n';
            }
        }
    }

    if (isRemote()) {
        if (currentCommand!=Status && (currentCommand!=Unknown || !currentArgs.isEmpty())) {
            return false;
        }

        #if QT_VERSION < 0x050000
        QUrl url(dynamicUrl+"/"+constSaveCmd);
        url.addQueryItem("name", e.name);
        url.addQueryItem(constStatusTime, QString::number(statusTime));
        #else
        QUrl url(dynamicUrl+"/"+constSaveCmd);
        QUrlQuery query;
        query.addQueryItem("name", e.name);
        query.addQueryItem(constStatusTime, QString::number(statusTime));
        url.setQuery(query);
        #endif

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
        currentJob=NetworkAccessManager::self()->post(req, string.toUtf8());
        connect(currentJob, SIGNAL(finished()), this, SLOT(remoteJobFinished()));
        currentCommand=Save;
        currentArgs.clear();
        currentSave=e;
        return true;
    }

    QFile f(Utils::configDir(constDir, true)+e.name+constExtension);
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
        sendCommand(Del, QStringList() << name);
        return;
    }

    QList<Dynamic::Entry>::Iterator it=find(name);
    if (it==entryList.end()) {
        return;
    }
    QString fName(Utils::configDir(constDir, false)+name+constExtension);
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
        sendCommand(SetActive, QStringList() << "name" << name << "start" << "1");
        return;
    }

    if (Utils::findExe("perl").isEmpty()) {
        emit error(i18n("You need to install \"perl\" on your system in order for Cantata's dynamic mode to function."));
        return;
    }

    QString fName(Utils::configDir(constDir, false)+name+constExtension);

    if (!QFile::exists(fName)) {
        emit error(i18n("Failed to locate rules file - %1").arg(fName));
        return;
    }

    QString rules(Utils::cacheDir(constDir, true)+constActiveRules);

    QFile::remove(rules);
    if (QFile::exists(rules)) {
        emit error(i18n("Failed to remove previous rules file - %1").arg(rules));
        return;
    }

    if (!QFile::link(fName, rules)) {
        emit error(i18n("Failed to install rules file - %1 -> %2").arg(fName).arg(rules));
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

void Dynamic::stop()
{
    if (isRemote()) {
        sendCommand(Control, QStringList() << "state" << "stop");
        return;
    }

    #if !defined Q_OS_WIN
    int i=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
    QModelIndex idx=index(i, 0, QModelIndex());
    int pid=getPid();

    if (!pid) {
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return;
    }

    if (0!=::kill(pid, 0)) {
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return;
    }

    if (controlApp(false)) {
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
    QString cmd=QLatin1String(INSTALL_PREFIX"/share/cantata/scripts/cantata-dynamic");
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
    QString dirName=Utils::configDir(constDir);
    QDir d(dirName);
    if (d.exists()) {
        QStringList rulesFiles=d.entryList(QStringList() << QChar('*')+constExtension);
        foreach (const QString &rf, rulesFiles) {
            QFile f(dirName+rf);
            if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QStringList keys=QStringList() << constArtistKey << constSimilarArtistsKey << constAlbumArtistKey << constDateKey << constExactKey
                                               << constAlbumKey << constTitleKey << constGenreKey << constExcludeKey;

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
                    } else {
                        foreach (const QString &k, keys) {
                            if (str.startsWith(k+':')) {
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

void Dynamic::loadRemote()
{
    beginResetModel();
    entryList.clear();
    currentEntry=QString();
    endResetModel();
    sendCommand(Id);
}

void Dynamic::parseRemote(const QString &response)
{
    beginResetModel();
    entryList.clear();
    currentEntry=QString();
    QStringList keys=QStringList() << constArtistKey << constSimilarArtistsKey << constAlbumArtistKey << constDateKey << constExactKey
                                   << constAlbumKey << constTitleKey << constGenreKey << constExcludeKey;
    QStringList list=response.split("\n");
    Entry e;
    Rule r;

    foreach (const QString &s, list) {
        QString str=s.trimmed();
        if (str.isEmpty() || str.startsWith('#')) {
            continue;
        }
        if (str.startsWith(QLatin1String("FILENAME:"))) {
            if (!e.name.isEmpty()) {
                entryList.append(e);
            }
            e.name=str.mid(9, str.length()-15); // Remove extension...
            e.rules.clear();
            r.clear();
        } else if (str==constRuleKey) {
            if (!r.isEmpty()) {
                e.rules.append(r);
                r.clear();
            }
        } else if (str.startsWith(constStatusTimeResp)) {
            statusTime=str.mid(constStatusTimeResp.length()).toInt();
        } else if (str.startsWith(constUpdateRequiredResp)) {
            continue;
        } else {
            foreach (const QString &k, keys) {
                if (str.startsWith(k+':')) {
                    r.insert(k, str.mid(k.length()+1));
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

void Dynamic::parseStatus(const QString &response)
{
    QStringList list=response.split("\n");
    QString state;
    QString prevEntry=currentEntry;
    int st=-1;
    foreach (const QString &str, list) {
        if (str.startsWith(constStatusStateResp)) {
            state=str.mid(constStatusStateResp.length());
        } else if (str.startsWith(constStatusRuleResp)) {
            currentEntry=str.mid(constStatusRuleResp.length());
        } else if (str.startsWith(constStatusTimeResp)) {
            st=str.mid(constStatusTimeResp.length()).toInt();
        }
    }

    bool stateChanged=lastState!=state;
    bool noSongs=QLatin1String("NO_SONGS")==state;
    bool terminated=false;

    if (stateChanged) {
        lastState=state;
        if (noSongs) {
            bool sendError=!currentEntry.isEmpty();
            emit running(true);
            if (sendError) {
                emit error(QLatin1String("NO_SONGS"));
            }
        } else if (QLatin1String("HAVE_SONGS")==state || QLatin1String("STARTING")==state) {
            emit running(true);
        } else if (QLatin1String("IDLE")==state) {
            currentEntry.clear();
            emit running(false);
        } else if (QLatin1String("TERMINATED")==state) {
            currentEntry.clear();
            emit running(false);
            emit error(i18n("Dynamizer has been terminated."));
            pollRemoteHelper();
            terminated=true;
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
        if (Unknown==currentCommand && currentArgs.isEmpty()) {
            statusTime=st;
            sendCommand(List, QStringList() << "withDetails" << "1");
        }
    }
}

void Dynamic::parseId(const QString &response)
{
    QStringList list=response.split("\n");
    QString id;
    quint16 port=0;
    QString group;

    foreach (const QString &param, list) {
        if (param.startsWith(QLatin1String("ID:"))) {
            id=param.mid(3);
        } else if (param.startsWith(QLatin1String("GROUP:"))) {
            group=param.mid(6);
        } else if (param.startsWith(QLatin1String("PORT:"))) {
            port=param.mid(5).toUInt();
        }
    }

    if (!id.isEmpty() && !group.isEmpty() && 0!=port) {
        startReceiver(id, group, port);
    }
}

void Dynamic::checkResponse(const QString &response)
{
    QStringList list=response.split("\n");
    bool updateReq=false;
    foreach (const QString &str, list) {
        if (constUpdateRequiredResp==str) {
            updateReq=true;
        } else if (str.startsWith(constStatusTimeResp)) {
            statusTime=str.mid(constStatusTimeResp.length()).toInt();
        }
    }

    if (updateReq) {
        sendCommand(List, QStringList() << "withDetails" << "1");
    }
}

void Dynamic::sendCommand(Command cmd, const QStringList &args)
{
    if (Status==currentCommand) {
        if (cmd==Status) {
            return;
        }
        currentCommand=Unknown;
        currentArgs.clear();
        if (currentJob) {
            disconnect(currentJob, SIGNAL(finished()), this, SLOT(remoteJobFinished()));
        }
    }
    if (Unknown!=currentCommand) {
        if (Status!=cmd) {
            QString cmdStr=i18n("Uknown");
            switch(cmd) {
            case List:      cmdStr=i18n("Loading list of rules"); break;
            case Save:      cmdStr=i18n("Saving rule"); break;
            case Del:       cmdStr=i18n("Deleting rule"); break;
            case SetActive: cmdStr=i18n("Setting active rule"); break;
            case Control:   cmdStr=i18n("Stopping dynamizer"); break;
            case Id:        cmdStr=i18n("Requesting ID details"); break;
            default: break;
            }
            emit error(i18n("Awaiting response for previous command. (%1)").arg(cmdStr));
        }
        return;
    }

    currentCommand=cmd;
    currentArgs=args;

    DBUG << toString(cmd) << args;
    if (Del==cmd) {
        if (!args.isEmpty()) {
            #if QT_VERSION < 0x050000
            QUrl url(dynamicUrl+"/"+args.at(0));
            url.addQueryItem(constStatusTime, QString::number(statusTime));
            #else
            QUrl url(dynamicUrl+"/"+args.at(0));
            QUrlQuery query;
            query.addQueryItem(constStatusTime, QString::number(statusTime));
            url.setQuery(query);
            #endif
        
            currentJob=NetworkAccessManager::self()->deleteResource(QNetworkRequest(url));
        } else {
            currentCommand=Unknown;
            currentArgs.clear();
        }
    } else {
        QUrl url(dynamicUrl+"/"+toString(currentCommand));
        #if QT_VERSION < 0x050000
        QUrl &query=url;
        #else
        QUrlQuery query;
        #endif
        if (!args.isEmpty() && 0==args.size()%2) {
            for (int i=0; i<args.size(); i+=2) {
                query.addQueryItem(args.at(i), args.at(i+1));
            }
        }
        query.addQueryItem(constStatusTime, QString::number(statusTime));
        #if QT_VERSION >= 0x050000
        url.setQuery(query);
        #endif
        if (SetActive==cmd || Control==cmd) {
            QNetworkRequest req(url);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
            currentJob=NetworkAccessManager::self()->post(req, QByteArray());
        } else {
            currentJob=NetworkAccessManager::self()->get(QNetworkRequest(url));
        }
    }
    if (List==cmd) {
        emit loadingList();
    }
    connect(currentJob, SIGNAL(finished()), this, SLOT(remoteJobFinished()));
}

void Dynamic::refreshList()
{
    if (isRemote()) {
        sendCommand(List, QStringList() << "withDetails" << "1");
    }
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
    remoteTimer->start(5000);
    emit remoteRunning(false);
}

void Dynamic::checkIfRemoteIsRunning()
{
    if (isRemote() && entryList.isEmpty()) {
        sendCommand(Id);
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

void Dynamic::remoteJobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply!=currentJob) {
        return;
    }

    currentJob=0;
    reply->deleteLater();

    int httpResponse=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool cmdOk=QNetworkReply::NoError==reply->error() && (200==httpResponse || 201==httpResponse);
    DBUG << toString(currentCommand) << currentArgs << cmdOk;
    QString response;
    if (cmdOk) {
        reply->open(QIODevice::ReadOnly | QIODevice::Text);
        response=QString::fromUtf8(reply->readAll());
        reply->close();
    } else {
        response=0==httpResponse ? i18n("Dynamizer is not active") : QString::number(httpResponse);
    }

    switch (currentCommand) {
    case List:
        if (cmdOk) {
            parseRemote(response);
        } else {
            emit error(i18n("Failed to retrieve list of dynamic rules. (%1)").arg(response));
        }
        emit loadedList();
        break;
    case Status:
        if (cmdOk) {
            parseStatus(response);
        } else {
            emit running(false);
        }
        break;
    case Save:
        if (cmdOk) {
            updateEntry(currentSave);
            currentSave.name=QString();
            currentSave.rules.clear();
        }
        checkResponse(response);
        emit saved(cmdOk);
        break;
    case Del:
        if (cmdOk) {
            QString name=currentArgs.isEmpty() ? QString() : currentArgs.at(0);
            QList<Dynamic::Entry>::Iterator it=find(name);
            if (it!=entryList.end()) {
                beginRemoveRows(QModelIndex(), it-entryList.begin(), it-entryList.begin());
                entryList.erase(it);
                endRemoveRows();
            }
            checkResponse(response);
        } else {
            emit error(i18n("Failed to delete rules file. (%1)").arg(response));
        }
        break;
    case Control:
        if (cmdOk) {
            checkResponse(response);
        } else {
            emit error(i18n("Failed to control dynamizer state. (%1)").arg(response));
        }
        lastState.clear();
        break;
    case SetActive:
        if (cmdOk) {
            checkResponse(response);
            QTimer::singleShot(1000, this, SLOT(updateRemoteStatus()));
        } else {
            emit error(i18n("Failed to set the current dynamic rules. (%1)").arg(response));
        }
        lastState.clear();
        break;
    case Id:
        if (cmdOk) {
            parseId(response);
            currentCommand=Unknown;
            currentArgs.clear();
            sendCommand(List, QStringList() << "withDetails" << "1");
            emit remoteRunning(true);
            return;
        }
    default:
        break;
    }

    Command prevCommand=currentCommand;
    currentCommand=Unknown;
    currentArgs.clear();

    if (cmdOk) {
        if (remoteTimer) {
            remoteTimer->stop();
        }
        if (Status!=prevCommand) {
            sendCommand(Status);
        }
    } else if (0==httpResponse) {
        pollRemoteHelper();
    }
}

void Dynamic::dynamicUrlChanged(const QString &url)
{
    if (url!=dynamicUrl) {
        dynamicUrl=url;
        stopReceiver();
        if (isRemote()) {
            if (localTimer) {
                localTimer->stop();
            }
            loadRemote();
        } else {
            if (remoteTimer) {
                remoteTimer->stop();
            }
            loadLocal();
        }
    }
}

void Dynamic::stopReceiver()
{
    if (receiver) {
        disconnect(receiver, SIGNAL(status(QString)), this, SLOT(parseStatus(QString)));
        receiver->deleteLater();
        receiver=0;
    }
}

void Dynamic::startReceiver(const QString &id, const QString &group, quint16 port)
{
    stopReceiver();
    receiver=new MulticastReceiver(this, id, group, port);
    connect(receiver, SIGNAL(status(QString)), this, SLOT(parseStatus(QString)));
}
