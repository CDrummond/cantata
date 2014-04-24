/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "localize.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <QThread>
#include <QApplication>
#include <QDateTime>
#include <QTime>
#include <QWidget>
#include <QStyle>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef Q_OS_WIN
#include <QDesktopServices>
#endif
#ifndef Q_OS_WIN
#include <grp.h>
#include <pwd.h>
#endif
#include <sys/types.h>
#include <utime.h>

#ifdef ENABLE_UBUNTU

// For Cantata touch, use rev-url for cache/config/data folders
#ifdef CANTATA_REV_URL
    #define FOLDER_NAME CANTATA_REV_URL
#else
    // TODO: **really** should be using this. But for some reason its blank????
    #define QCoreApplication::organizationName()
#endif

#else

#define FOLDER_NAME QCoreApplication::applicationName()

#endif

const QLatin1Char Utils::constDirSep('/');
const QLatin1String Utils::constDirSepStr("/");
const char * Utils::constDirSepCharStr="/";

static const QLatin1String constHttp("http://");

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

QString Utils::stripAcceleratorMarkers(QString label)
{
    int p = 0;
    forever {
        p = label.indexOf('&', p);
        if(p < 0 || p + 1 >= label.length()) {
            break;
        }

        if(label.at(p + 1).isLetterOrNumber() || label.at(p + 1) == '&') {
            label.remove(p, 1);
        }

        ++p;
    }
    return label;
}

QString Utils::fixPath(const QString &dir)
{
    QString d(dir);

    if (!d.isEmpty() && !d.startsWith(constHttp)) {
        #ifdef Q_OS_WIN
        // Windows shares can be \\host\share (which gets converted to //host/share)
        // so if th epath starts with // we need to keep this double slash.
        bool startsWithDoubleSlash=d.length()>2 && d.startsWith(QLatin1String("//"));
        #endif
        d.replace(QLatin1String("//"), constDirSepStr);
        #ifdef Q_OS_WIN
        if (startsWithDoubleSlash) { // Re add first slash
            d=QLatin1Char('/')+d;
        }
        #endif
    }
    d.replace(QLatin1String("/./"), constDirSepStr);
    if (!d.isEmpty() && !d.endsWith(constDirSep)) {
        d+=constDirSep;
    }
    return d;
}

QString Utils::convertDirForDisplay(const QString &dir)
{
    if (dir.isEmpty() || dir.startsWith(constHttp)) {
        return dir;
    }

    QString d(dir);
    if (d.endsWith(constDirSep)) {
        d=d.left(d.length()-1);
    }
    return QDir::toNativeSeparators(d);
}

QString Utils::convertDirFromDisplay(const QString &dir)
{
    QString d=dir.trimmed();
    if (d.isEmpty()) {
        return d;
    }

    if (d.startsWith(constHttp)) {
        return fixPath(d);
    }
    return fixPath(QDir::fromNativeSeparators(d));
}

QString Utils::getDir(const QString &file)
{
    QString d(file);
    int slashPos(d.lastIndexOf(constDirSep));

    if(slashPos!=-1) {
        d.remove(slashPos+1, d.length());
    }

    return fixPath(d);
}

QString Utils::getFile(const QString &file)
{
    QString d(file);
    int slashPos=d.lastIndexOf(constDirSep);

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

    // First of all see if current group is actually 'groupName'!!!
    gid_t egid=getegid();
    struct group *group=getgrgid(egid);

    if (group && 0==strcmp(group->gr_name, groupName)) {
        gid=egid;
        return gid;
    }

    // Now see if user is a member of 'groupName'
    struct passwd *pw=getpwuid(geteuid());

    if (!pw) {
        return gid;
    }

    group=getgrnam(groupName);

    if (group) {
        for (int i=0; group->gr_mem[i]; ++i) {
            if (0==strcmp(group->gr_mem[i], pw->pw_name)) {
                gid=group->gr_gid;
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
bool Utils::createWorldReadableDir(const QString &dir, const QString &base, const char *groupName)
{
    #ifdef Q_OS_WIN
    Q_UNUSED(base);
    Q_UNUSED(groupName);
    return makeDir(dir, 0775);
    #else
    //
    // Clear any umask before dir is created
    mode_t oldMask(umask(0000));
    gid_t gid=base.isEmpty() ? 0 : getGroupId(groupName);
    bool status(makeDir(dir, 0==gid ? 0755 : 0775));

    if (status && 0!=gid && dir.startsWith(base)) {
        QStringList parts=dir.mid(base.length()).split(constDirSep);
        QString d(base);

        foreach (const QString &p, parts) {
            d+=constDirSep+p;
            int rv=::chown(QFile::encodeName(d).constData(), geteuid(), gid);
            Q_UNUSED(rv);
        }
    }
    // Reset umask
    ::umask(oldMask);
    return status;
    #endif
}

struct Thread : public QThread
{
    using QThread::msleep;
};

void Utils::msleep(int msecs)
{
    Thread::msleep(msecs);
}

#ifndef ENABLE_KDE_SUPPORT
// Copied from KDE... START
#include <QLocale>
#include <fcntl.h>
#include <sys/stat.h>

// kde_file.h
#ifndef Q_WS_WIN
#if (defined _LFS64_LARGEFILE) && (defined _LARGEFILE64_SOURCE) && (!defined _GNU_SOURCE) && (!defined __sun)
#define KDE_stat                ::stat64
#define KDE_lstat               ::lstat64
#define KDE_struct_stat         struct stat64
#define KDE_mkdir               ::mkdir
#else
#define KDE_stat                ::stat
#define KDE_lstat               ::lstat
#define KDE_struct_stat         struct stat
#define KDE_mkdir               ::mkdir
#endif
#endif // Q_WS_WIN

// kstandarddirs.h
bool Utils::makeDir(const QString &dir, int mode)
{
    // we want an absolute path
    if (QDir::isRelativePath(dir)) {
        return false;
    }

    #ifdef Q_WS_WIN
    Q_UNUSED(mode)
    return QDir().mkpath(dir);
    #else
    QString target = dir;
    uint len = target.length();

    // append trailing slash if missing
    if (dir.at(len - 1) != QLatin1Char('/')) {
        target += QLatin1Char('/');
    }

    QString base;
    uint i = 1;

    while ( i < len ) {
        KDE_struct_stat st;
        int pos = target.indexOf(QLatin1Char('/'), i);
        base += target.mid(i - 1, pos - i + 1);
        QByteArray baseEncoded = QFile::encodeName(base);
        // bail out if we encountered a problem
        if (KDE_stat(baseEncoded, &st) != 0) {
            // Directory does not exist....
            // Or maybe a dangling symlink ?
            if (KDE_lstat(baseEncoded, &st) == 0)
                (void)unlink(baseEncoded); // try removing

            if (KDE_mkdir(baseEncoded, static_cast<mode_t>(mode)) != 0) {
                baseEncoded.prepend( "trying to create local folder " );
                perror(baseEncoded.constData());
                return false; // Couldn't create it :-(
            }
        }
        i = pos + 1;
    }
    return true;
    #endif
}

QString Utils::formatByteSize(double size)
{
    static bool useSiUnites=false;
    static QLocale locale;

    #ifndef Q_OS_WIN
    static bool init=false;
    if (!init) {
        init=true;
        const char *env=qgetenv("KDE_FULL_SESSION");
        QString dm=env && 0==strcmp(env, "true") ? QLatin1String("KDE") : QString(qgetenv("XDG_CURRENT_DESKTOP"));
        useSiUnites=!dm.isEmpty() && QLatin1String("KDE")!=dm;
    }
    #endif
    int unit = 0;
    double multiplier = useSiUnites ? 1000.0 : 1024.0;

    while (qAbs(size) >= multiplier && unit < 3) {
        size /= multiplier;
        unit++;
    }

    if (useSiUnites) {
        switch(unit) {
        case 0: return i18n("%1 B", size);
        case 1: return i18n("%1 kB", locale.toString(size, 'f', 1));
        case 2: return i18n("%1 MB", locale.toString(size, 'f', 1));
        default:
        case 3: return i18n("%1 GB", locale.toString(size, 'f', 1));
        }
    } else {
        switch(unit) {
        case 0: return i18n("%1 B", size);
        case 1: return i18n("%1 KiB", locale.toString(size, 'f', 1));
        case 2: return i18n("%1 MiB", locale.toString(size, 'f', 1));
        default:
        case 3: return i18n("%1 GiB", locale.toString(size, 'f', 1));
        }
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
    #ifdef Q_OS_WIN
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
    if ((file = fopen(info.absoluteFilePath().toUtf8().constData(), "r"))) {
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
    if ((file = fopen(orig.absoluteFilePath().toUtf8().constData(), "r"))) {
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

QString Utils::formatDuration(const quint32 totalseconds)
{
    //Get the days,hours,minutes and seconds out of the total seconds
    quint32 days = totalseconds / 86400;
    quint32 rest = totalseconds - (days * 86400);
    quint32 hours = rest / 3600;
    rest = rest - (hours * 3600);
    quint32 minutes = rest / 60;
    quint32 seconds = rest - (minutes * 60);

    //Convert hour,minutes and seconds to a QTime for easier parsing
    QTime time(hours, minutes, seconds);

    #ifdef ENABLE_KDE_SUPPORT
    return 0==days
            ? time.toString("h:mm:ss")
            : i18np("1 day %2", "%1 days %2", days, time.toString("h:mm:ss"));
    #else
    return 0==days
            ? time.toString("h:mm:ss")
            : QString("%1:%2").arg(days).arg(time.toString("hh:mm:ss"));
    #endif
}

QString Utils::formatTime(const quint32 seconds, bool zeroIsUnknown)
{
    if (0==seconds && zeroIsUnknown) {
        return i18n("Unknown");
    }

    static const quint32 constHour=60*60;
    if (seconds>constHour) {
        return Utils::formatDuration(seconds);
    }

    QString result(QString::number(floor(seconds / 60.0))+QChar(':'));
    if (seconds % 60 < 10) {
        result += "0";
    }
    return result+QString::number(seconds % 60);
}

QString Utils::cleanPath(const QString &p)
{
    QString path(p);
    while (path.contains("//")) {
        path.replace("//", constDirSepStr);
    }
    return fixPath(path);
}

static QString userDir(const QString &mainDir, const QString &sub, bool create)
{
    QString dir=mainDir;
    if (!sub.isEmpty()) {
        dir+=sub;
    }
    dir=Utils::cleanPath(dir);
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? dir : QString();
}

QString Utils::configDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN
    return userDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation)+constDirSep, sub, create);
    #else
    QString env = qgetenv("XDG_CONFIG_HOME");
    return userDir((env.isEmpty() ? QDir::homePath() + "/.config" : env) + constDirSep+FOLDER_NAME+constDirSep, sub, create);
    #endif
}

QString Utils::dataDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN
    return userDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation)+constDirSep, sub, create);
    #else
    QString env = qgetenv("XDG_DATA_HOME");
    return userDir((env.isEmpty() ? QDir::homePath() + "/.local/share" : env) + constDirSep+FOLDER_NAME+constDirSep, sub, create);
    #endif
}

QString Utils::cacheDir(const QString &sub, bool create)
{
    #if defined Q_OS_WIN
    return userDir(QDesktopServices::storageLocation(QDesktopServices::CacheLocation)+constDirSep, sub, create);
    #else
    QString env = qgetenv("XDG_CACHE_HOME");
    return userDir((env.isEmpty() ? QDir::homePath() + "/.cache" : env) + constDirSep+FOLDER_NAME+constDirSep, sub, create);
    #endif
}

bool Utils::moveFile(const QString &from, const QString &to)
{
    return !from.isEmpty() && !to.isEmpty() && from!=to && QFile::exists(from) && !QFile::exists(to) && QFile::rename(from, to);
}

void Utils::moveDir(const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty() || from==to) {
        return;
    }

    QDir f(from);
    if (!f.exists()) {
        return;
    }

    QDir t(to);
    if (!t.exists()) {
        return;
    }

    QFileInfoList files=f.entryInfoList(QStringList() << "*", QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);
    foreach (const QFileInfo &file, files) {
        if (file.isDir()) {
            QString dest=to+file.fileName()+constDirSep;
            if (!QDir(dest).exists()) {
                t.mkdir(file.fileName());
            }
            moveDir(from+file.fileName()+constDirSep, dest);
        } else {
            QFile::rename(from+file.fileName(), to+file.fileName());
        }
    }

    f.cdUp();
    f.rmdir(from);
}

void Utils::clearOldCache(const QString &sub, int maxAge)
{
    if (sub.isEmpty()) {
        return;
    }

    QString d=cacheDir(sub, false);
    if (d.isEmpty()) {
        return;
    }

    QDir dir(d);
    if (dir.exists()) {
        QFileInfoList files=dir.entryInfoList(QDir::Files|QDir::NoDotAndDotDot);
        if (files.count()) {
            QDateTime now=QDateTime::currentDateTime();
            foreach (const QFileInfo &f, files) {
                if (f.lastModified().daysTo(now)>maxAge) {
                    QFile::remove(f.absoluteFilePath());
                }
            }
        }
    }
}

void Utils::touchFile(const QString &fileName)
{
    ::utime(QFile::encodeName(fileName).constData(), 0);
}

bool Utils::isDirReadable(const QString &dir)
{
    #ifdef Q_OS_WIN32
    if (dir.isEmpty()) {
        return false;
    } else {
        QDir d(dir);
        bool dirReadable=d.isReadable();
        // Handle cases where dir is set to \\server\ (i.e. no shared folder is set in path)
        if (!dirReadable && dir.startsWith(QLatin1String("//")) && d.isRoot() && (dir.length()-1)==dir.indexOf(Utils::constDirSep, 2)) {
            dirReadable=true;
        }
        return dirReadable;
    }
    #else
    return dir.isEmpty() ? false : QDir(dir).isReadable();
    #endif
}

double Utils::smallFontFactor(const QFont &f)
{
    double sz=f.pointSizeF();
    #if defined Q_OS_WIN32
    if (sz<8.5) {
        return 1.0;
    }
    if (sz<9.0) {
        return 0.9;
    }
    return 0.85;
    #else
    if (sz<7.0) {
        return 1.0;
    }
    if (sz<8.5) {
        return 0.9;
    }
    if (sz<9.0) {
        return 0.85;
    }
    return 0.8;
    #endif
}

QFont Utils::smallFont(QFont f)
{
    f.setPointSizeF(f.pointSizeF()*smallFontFactor(f));
    return f;
}

int Utils::layoutSpacing(QWidget *w)
{
    int spacing=(w ? w->style() : qApp->style())->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
    if (spacing<0) {
        spacing=isHighDpi() ? 8 : 4;
    }
    return spacing;
}

bool Utils::isHighDpi()
{
    static int fontHeight=-1;
    if (-1==fontHeight) {
        fontHeight=QApplication::fontMetrics().height();
    }
    return fontHeight>22;
}

Utils::Desktop Utils::currentDe()
{
    #if !defined Q_OS_WIN32 && !defined Q_OS_MAC
    static int de=-1;
    if (-1==de) {
        de=Other;
        QByteArray desktop=qgetenv("XDG_CURRENT_DESKTOP").toLower();
        if ("unity"==desktop) {
            de=Unity;
        } else if ("kde"==desktop) {
            de=KDE;
        } else if ("gnome"==desktop || "pantheon"==desktop) {
            de=Gnome;
        } else {
            QByteArray kde=qgetenv("KDE_FULL_SESSION");
            if ("true"==kde) {
                de=KDE;
            }
        }
    }
    return (Utils::Desktop)de;
    #endif
    return Other;
}

static bool isTouchFriendly=false;
void Utils::setTouchFriendly(bool t)
{
    isTouchFriendly=t;
}

bool Utils::touchFriendly()
{
    return isTouchFriendly;
}
