/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "itemview.h"
#include "network.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#include <KDE/KMessageBox>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(Dynamic, instance)
#endif
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QProcess>
#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <signal.h>

static const QString constDir=QLatin1String("dynamic");
static const QString constExtension=QLatin1String(".rules");
static const QString constActiveRules=QLatin1String("rules");
static const QString constLockFile=QLatin1String("lock");

static QString configDir(const QString &sub, bool create=false)
{
    QString env = qgetenv("XDG_CONFIG_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + QLatin1String("/.config") : env) + QLatin1String("/"PACKAGE_NAME"/");
    if(!sub.isEmpty()) {
        dir+=sub;
    }
    dir=MPDParseUtils::fixPath(dir);
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? QDir::toNativeSeparators(dir) : QString();
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
const QString Dynamic::constAlbumArtistKey=QLatin1String("AlbumArtist");
const QString Dynamic::constAlbumKey=QLatin1String("Album");
const QString Dynamic::constTitleKey=QLatin1String("Title");
const QString Dynamic::constGenreKey=QLatin1String("Genre");
const QString Dynamic::constDateKey=QLatin1String("Date");
const QString Dynamic::constExactKey=QLatin1String("Exact");
const QString Dynamic::constExcludeKey=QLatin1String("Exclude");

Dynamic::Dynamic()
    : timer(0)
{
    // Load all current enttries...
    QString dirName=configDir(constDir);
    QDir d(dirName);
    if (d.exists()) {
        QStringList rulesFiles=d.entryList(QStringList() << QChar('*')+constExtension);
        foreach (const QString &rf, rulesFiles) {
            QFile f(dirName+rf);
            if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
                QStringList keys=QStringList() << constArtistKey << constAlbumArtistKey << constDateKey << constExactKey
                                               << constAlbumKey << constTitleKey << constGenreKey << constExcludeKey;

                Entry e;
                e.name=rf.left(rf.length()-constExtension.length());
                Rule r;
                while (!f.atEnd()) {
                    QString str=f.readLine().trimmed();

                    if (str.isEmpty() && str.startsWith('#')) {
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

    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    QTimer::singleShot(0, this, SLOT(checkHelper()));
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
        return !currentEntry.isEmpty() && entryList.at(index.row()).name==currentEntry ? QIcon::fromTheme("media-playback-start") : QIcon::fromTheme("media-playlist-shuffle");
    case ItemView::Role_SubText: {
        #ifdef ENABLE_KDE_SUPPORT
        return i18np("1 Rule", "%1 Rules", entryList.at(index.row()).rules.count());
        #else
        int count=entryList.at(index.row()).rules.count();
        return count!=1 ? tr("%1 Rules").arg(count) : tr("1 Rule");
        #endif
    }
    case ItemView::Role_ToggleIconName:
        return !currentEntry.isEmpty() && entryList.at(index.row()).name==currentEntry ? "process-stop" : "media-playback-start";
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

    QFile f(configDir(constDir, true)+e.name+constExtension);
    if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream str(&f);
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
        return true;
    }
    return false;
}

bool Dynamic::del(const QString &name)
{
    QList<Dynamic::Entry>::Iterator it=find(name);
    if (it==entryList.end()) {
        return true;
    }
    QString fName(configDir(constDir, false)+name+constExtension);

    if (!QFile::exists(fName) || QFile::remove(fName)) {
        beginRemoveRows(QModelIndex(), it-entryList.begin(), it-entryList.begin());
        entryList.erase(it);
        endRemoveRows();
        return true;
    }
    return false;
}

bool Dynamic::start(const QString &name)
{
    QString fName(configDir(constDir, false)+name+constExtension);

    if (!QFile::exists(fName)) {
        #ifdef ENABLE_KDE_SUPPORT
        emit error(i18n("Failed to locate rules file - %1", fName));
        #else
        emit error(tr("Failed to locate rules file - %1").arg(fName));
        #endif
        return false;
    }

    QString rules(Network::cacheDir(constDir, true)+constActiveRules);

    if (QFile::exists(rules) && !QFile::remove(rules)) {
        #ifdef ENABLE_KDE_SUPPORT
        emit error(i18n("Failed to remove previous rules file - %1", rules));
        #else
        emit error(tr("Failed to remove previous rules file - %1").arg(rules));
        #endif
        return false;
    }

    if (!QFile::link(fName, rules)) {
        #ifdef ENABLE_KDE_SUPPORT
        emit error(i18n("Failed to install rules file - %1 -> %2", fName, rules));
        #else
        emit error(tr("Failed to install rules file - %1 -> %2").arg(fName).arg(rules));
        #endif
        return false;
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
        return true;
    }
    if (controlApp(true)) {
        emit running(isRunning());
        emit clear();
        return true;
    }
    return false;
}

bool Dynamic::stop()
{
    int i=currentEntry.isEmpty() ? -1 : entryList.indexOf(currentEntry);
    QModelIndex idx=index(i, 0, QModelIndex());
    int pid=getPid();

    if (!pid) {
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return true;
    }

    if (0!=::kill(pid, 0)) {
        currentEntry=QString();
        emit running(false);
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return true;
    }

    if (controlApp(false)) {
        currentEntry=QString();
        emit running(isRunning());
        if (idx.isValid()) {
            emit dataChanged(idx, idx);
        }
        return true;
    }
    return false;
}

bool Dynamic::toggle(const QString &name)
{
    return name==currentEntry ? stop() : start(name);
}

bool Dynamic::isRunning()
{
    int pid=getPid();
    return pid ? 0==::kill(pid, 0) : false;
}

int Dynamic::getPid() const
{
    QFile pidFile(Network::cacheDir(constDir, false)+constLockFile);

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
    #ifdef ENABLE_KDE_SUPPORT
    QString cmd=KStandardDirs::findExe("cantata-dynamic", KStandardDirs::installPath("libexec"));

    if (cmd.isEmpty()) {
        emit error(i18n("Failed to locate the 'cantata-dynamic' help script."));
        return false;
    }
    #else
    QString cmd=QLatin1String("cantata-dynamic");
    #endif

    QProcess process;

    if (isStart) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString p=Settings::self()->connectionPasswd();
        env.insert("MPD_HOST", p.isEmpty() ? Settings::self()->connectionHost() : (p+'@'+Settings::self()->connectionHost()));
        env.insert("MPD_PORT", QString::number(Settings::self()->connectionPort()));
        process.setProcessEnvironment(env);
    }
    process.start(cmd, QStringList() << QLatin1String(isStart ? "start" : "stop"), QIODevice::WriteOnly);

    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), SLOT(checkHelper()));
    }
    bool rv=process.waitForFinished(1000);
    timer->start(1000);
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

void Dynamic::checkHelper()
{
    if (!isRunning()) {
        emit running(false);
        if (timer) {
            timer->stop();
        }
    } else {
        if (timer) {
            static const int constAppCheck=15*1000;
            if (timer->interval()<constAppCheck) {
                timer->start(constAppCheck);
            }
        } else { // No timer => app startup!
            // Attempt to read current name...
            QFileInfo inf(Network::cacheDir(constDir, false)+constActiveRules);

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