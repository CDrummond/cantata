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

#include "utils.h"
#include "config.h"
#include "mpdparseutils.h"
#include "covers.h"
#include "localize.h"
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef Q_OS_WIN
#include <QtGui/QDesktopServices>
#endif
#ifndef Q_OS_WIN
#include <grp.h>
#include <pwd.h>
#endif

QString Utils::strippedText(QString s)
{
    s.remove(QString::fromLatin1("..."));
    int i = 0;
    while (i < s.size()) {
        ++i;
        if (s.at(i - 1) != QLatin1Char('&')) {
            continue;
        }

        if (i < s.size() && s.at(i) == QLatin1Char('&')) {
            ++i;
        }
        s.remove(i - 1, 1);
    }
    return s.trimmed();
}

QString Utils::fixPath(const QString &dir)
{
    QString d(dir);

    if (!d.isEmpty() && !d.startsWith(QLatin1String("http://"))) {
        d.replace(QLatin1String("//"), QChar('/'));
    }
    d.replace(QLatin1String("/./"), QChar('/'));
    if (!d.isEmpty() && !d.endsWith('/')) {
        d+='/';
    }
    return d;
}

QString Utils::getDir(const QString &file)
{
    QString d(file);

    int slashPos(d.lastIndexOf('/'));

    if(slashPos!=-1) {
        d.remove(slashPos+1, d.length());
    }

    return fixPath(d);
}

QString Utils::getFile(const QString &file)
{
    QString d(file);
    int slashPos=d.lastIndexOf('/');

    if (-1!=slashPos) {
        d.remove(0, slashPos+1);
    }

    return d;
}

QString Utils::changeExtension(const QString &file, const QString &extension)
{
    if (extension.isEmpty()) {
        return file;
    }

    QString f(file);
    int pos=f.lastIndexOf('.');
    if (pos>1) {
        f=f.left(pos+1);
    }

    if (f.endsWith('.')) {
        return f+(extension.startsWith('.') ? extension.mid(1) : extension);
    }
    return f+(extension.startsWith('.') ? extension : (QChar('.')+extension));
}

#ifndef Q_OS_WIN
gid_t Utils::getGroupId(const char *groupName)
{
    static bool init=false;
    static gid_t gid=0;

    if (init) {
        return gid;
    }

    init=true;

    struct passwd *pw=getpwuid(geteuid());

    if (!pw) {
        return gid;
    }

    struct group *audioGroup=getgrnam(groupName);

    if (audioGroup) {
        for (int i=0; audioGroup->gr_mem[i]; ++i) {
            if (0==strcmp(audioGroup->gr_mem[i], pw->pw_name)) {
                gid=audioGroup->gr_gid;
                return gid;
            }
        }
    }
    return gid;
}

/*
 * Set file permissions.
 * If user is a memeber of "users" group, then set file as owned by and writeable by "users" group.
 */
void Utils::setFilePerms(const QString &file, const char *groupName)
{
    //
    // Clear any umask before setting file perms
    mode_t oldMask(umask(0000));
    gid_t gid=getGroupId(groupName);
    QByteArray fn=QFile::encodeName(file);
    ::chmod(fn.constData(), 0==gid ? 0644 : 0664);
    if (0!=gid) {
        int rv=::chown(fn.constData(), geteuid(), gid);
        Q_UNUSED(rv);
    }
    // Reset umask
    ::umask(oldMask);
}
#else
void Utils::setFilePerms(const QString &file, const char *groupName)
{
    Q_UNUSED(file);
    Q_UNUSED(groupName);
}
#endif

/*
 * Create directory, and set its permissions.
 * If user is a memeber of "audio" group, then set dir as owned by and writeable by "audio" group.
 */
bool Utils::createDir(const QString &dir, const QString &base, const char *groupName)
{
    #ifdef Q_OS_WIN
    Q_UNUSED(base);
    Q_UNUSED(groupName);
    #else
    //
    // Clear any umask before dir is created
    mode_t oldMask(umask(0000));
    gid_t gid=base.isEmpty() ? 0 : getGroupId(groupName);
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    bool status(KStandardDirs::makeDir(dir, 0==gid ? 0755 : 0775));
    #else
    bool status(QDir(dir).mkpath(dir));
    #endif
    #ifndef Q_OS_WIN
    if (status && 0!=gid && dir.startsWith(base)) {
        QStringList parts=dir.mid(base.length()).split('/');
        QString d(base);

        foreach (const QString &p, parts) {
            d+='/'+p;
            int rv=::chown(QFile::encodeName(d).constData(), geteuid(), gid);
            Q_UNUSED(rv);
        }
    }
    // Reset umask
    ::umask(oldMask);
    #endif
    return status;
}

struct Thread : public QThread
{
    using QThread::msleep;
};

void Utils::msleep(int msecs)
{
    Thread::msleep(msecs);
}

void Utils::stopThread(QThread *thread)
{
    thread->quit();
    for(int i=0; i<10 && thread->isRunning(); ++i) {
        sleep();
    }
    thread->deleteLater();
}

#ifndef ENABLE_KDE_SUPPORT
// Copied from KDE... START
#include <QtCore/QLocale>

QString Utils::formatByteSize(double size)
{
    static QLocale locale;

    int unit = 0;
    double multiplier = 1024.0;

    while (qAbs(size) >= multiplier && unit < 3) {
        size /= multiplier;
        unit++;
    }

    switch(unit) {
    case 0: return i18n("%1 B").arg(size);
    case 1: return i18n("%1 KiB").arg(locale.toString(size, 'f', 1));
    case 2: return i18n("%1 MiB").arg(locale.toString(size, 'f', 1));
    default:
    case 3: return i18n("%1 GiB").arg(locale.toString(size, 'f', 1));
    }
}

#if defined Q_OS_WIN
#define KPATH_SEPARATOR ';'
// #define KDIR_SEPARATOR '\\' /* faster than QDir::separator() */
#else
#define KPATH_SEPARATOR ':'
// #define KDIR_SEPARATOR '/' /* faster than QDir::separator() */
#endif

static inline QString equalizePath(QString &str)
{
    #ifdef Q_WS_WIN
    // filter pathes through QFileInfo to have always
    // the same case for drive letters
    QFileInfo f(str);
    if (f.isAbsolute())
        return f.absoluteFilePath();
    else
    #endif
        return str;
}

static void tokenize(QStringList &tokens, const QString &str, const QString &delim)
{
    const int len = str.length();
    QString token;

    for(int index = 0; index < len; index++) {
        if (delim.contains(str[index])) {
            tokens.append(equalizePath(token));
            token.clear();
        } else {
            token += str[index];
        }
    }
    if (!token.isEmpty()) {
        tokens.append(equalizePath(token));
    }
}

#ifdef Q_OS_WIN
static QStringList executableExtensions()
{
    QStringList ret = QString::fromLocal8Bit(qgetenv("PATHEXT")).split(QLatin1Char(';'));
    if (!ret.contains(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        // If %PATHEXT% does not contain .exe, it is either empty, malformed, or distorted in ways that we cannot support, anyway.
        ret.clear();
        ret << QLatin1String(".exe")
            << QLatin1String(".com")
            << QLatin1String(".bat")
            << QLatin1String(".cmd");
    }
    return ret;
}
#endif

static QStringList systemPaths(const QString &pstr)
{
    QStringList tokens;
    QString p = pstr;

    if( p.isEmpty() ) {
        p = QString::fromLocal8Bit( qgetenv( "PATH" ) );
    }

    QString delimiters(QLatin1Char(KPATH_SEPARATOR));
    delimiters += QLatin1Char('\b');
    tokenize( tokens, p, delimiters );

    QStringList exePaths;

    // split path using : or \b as delimiters
    for( int i = 0; i < tokens.count(); i++ ) {
        exePaths << /*KShell::tildeExpand(*/ tokens[ i ] /*)*/; // TODO
    }

    return exePaths;
}

#ifdef Q_WS_MAC
static QString getBundle(const QString &path)
{
    //kDebug(180) << "getBundle(" << path << ", " << ignore << ") called";
    QFileInfo info;
    QString bundle = path;
    bundle += QLatin1String(".app/Contents/MacOS/") + bundle.section(QLatin1Char('/'), -1);
    info.setFile( bundle );
    FILE *file;
    if (file = fopen(info.absoluteFilePath().toUtf8().constData(), "r")) {
        fclose(file);
        struct stat _stat;
        if ((stat(info.absoluteFilePath().toUtf8().constData(), &_stat)) < 0) {
            return QString();
        }
        if ( _stat.st_mode & S_IXUSR ) {
            if ( ((_stat.st_mode & S_IFMT) == S_IFREG) || ((_stat.st_mode & S_IFMT) == S_IFLNK) ) {
                //kDebug(180) << "getBundle(): returning " << bundle;
                return bundle;
            }
        }
    }
    return QString();
}
#endif

static QString checkExecutable( const QString& path )
{
    #ifdef Q_WS_MAC
    QString bundle = getBundle( path );
    if ( !bundle.isEmpty() ) {
        //kDebug(180) << "findExe(): returning " << bundle;
        return bundle;
    }
    #endif
    QFileInfo info( path );
    QFileInfo orig = info;
    #if defined(Q_OS_DARWIN) || defined(Q_OS_MAC)
    FILE *file;
    if (file = fopen(orig.absoluteFilePath().toUtf8().constData(), "r")) {
        fclose(file);
        struct stat _stat;
        if ((stat(orig.absoluteFilePath().toUtf8().constData(), &_stat)) < 0) {
            return QString();
        }
        if ( _stat.st_mode & S_IXUSR ) {
            if ( ((_stat.st_mode & S_IFMT) == S_IFREG) || ((_stat.st_mode & S_IFMT) == S_IFLNK) ) {
                orig.makeAbsolute();
                return orig.filePath();
            }
        }
    }
    return QString();
    #else
    if( info.exists() && info.isSymLink() )
        info = QFileInfo( info.canonicalFilePath() );
    if( info.exists() && info.isExecutable() && info.isFile() ) {
        // return absolute path, but without symlinks resolved in order to prevent
        // problems with executables that work differently depending on name they are
        // run as (for example gunzip)
        orig.makeAbsolute();
        return orig.filePath();
    }
    //kDebug(180) << "checkExecutable(): failed, returning empty string";
    return QString();
    #endif
}

QString Utils::findExe(const QString &appname, const QString &pstr)
{
    #ifdef Q_OS_WIN
    QStringList executable_extensions = executableExtensions();
    if (!executable_extensions.contains(appname.section(QLatin1Char('.'), -1, -1, QString::SectionIncludeLeadingSep), Qt::CaseInsensitive)) {
        QString found_exe;
        foreach (const QString& extension, executable_extensions) {
            found_exe = findExe(appname + extension, pstr);
            if (!found_exe.isEmpty()) {
                return found_exe;
            }
        }
        return QString();
    }
    #endif

    const QStringList exePaths = systemPaths( pstr );
    for (QStringList::ConstIterator it = exePaths.begin(); it != exePaths.end(); ++it) {
        QString p = (*it) + QLatin1Char('/');
        p += appname;

        QString result = checkExecutable(p);
        if (!result.isEmpty()) {
            return result;
        }
    }

    return QString();
}
// Copied from KDE... END

#endif

QString Utils::cleanPath(const QString &p)
{
    QString path(p);
    while(path.contains("//")) {
        path.replace("//", "/");
    }
    return fixPath(path);
}

QString Utils::configDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation)+"/";
    #else
    QString env = qgetenv("XDG_CONFIG_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.config/" : env) + "/"+QCoreApplication::applicationName()+"/";
    #endif
    if(!sub.isEmpty()) {
        dir+=sub;
    }
    dir=Utils::fixPath(dir);
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? QDir::toNativeSeparators(dir) : QString();
}

QString Utils::cacheDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN
    QString dir = QDesktopServices::storageLocation(QDesktopServices::CacheLocation)+"/";
    #else
    QString env = qgetenv("XDG_CACHE_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.cache/" : env)+ "/"+QCoreApplication::applicationName()+"/";
    #endif
    if(!sub.isEmpty()) {
        dir+=sub;
    }
    dir=Utils::fixPath(dir);
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? QDir::toNativeSeparators(dir) : QString();
}
