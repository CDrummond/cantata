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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QSize>
#include <QDateTime>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KConfig>
#else
#include <QSettings>
#endif
#include "utils.h"

class Configuration
    #ifndef ENABLE_KDE_SUPPORT
    : public QSettings
    #endif
{
public:
    static QLatin1String constMainGroup;
    Configuration(const QString &group=constMainGroup);
    ~Configuration();
    
    #ifdef ENABLE_KDE_SUPPORT
    QString      get(const QString &key, const QString &def)     { return cfg->readEntry(key, def); }
    QStringList  get(const QString &key, const QStringList &def) { return cfg->readEntry(key, def); }
    bool         get(const QString &key, bool def)               { return cfg->readEntry(key, def); }
    int          get(const QString &key, int def)                { return cfg->readEntry(key, def); }
    unsigned int get(const QString &key, unsigned int def)       { return cfg->readEntry(key, def); }
    QByteArray   get(const QString &key, const QByteArray &def)  { return cfg->readEntry(key, def); }
    QSize        get(const QString &key, const QSize &def)       { return cfg->readEntry(key, def); }
    QDateTime    get(const QString &key, const QDateTime &def)   { return cfg->readEntry(key, def); }
    void         set(const QString &key, const QString &val)     { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, const char *val)        { if (!hasEntry(key) || get(key, QLatin1String(val))!=QLatin1String(val)) cfg->writeEntry(key, val); }
    void         set(const QString &key, const QStringList &val) { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, bool val)               { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, int val)                { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, unsigned int val)       { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, const QByteArray &val)  { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, const QSize &val)       { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    void         set(const QString &key, const QDateTime &val)   { if (!hasEntry(key) || get(key, val)!=val) cfg->writeEntry(key, val); }
    bool         hasGroup(const QString &grp)                    { return KGlobal::config()->hasGroup(grp); }
    void         removeGroup(const QString &grp)                 { KGlobal::config()->deleteGroup(grp); }
    bool         hasEntry(const QString &key)                    { return cfg->hasKey(key); }
    void         removeEntry(const QString &key)                 { cfg->deleteEntry(key); }
    void         beginGroup(const QString &group);
    void         endGroup();
    QStringList  childGroups()                                   { return cfg->groupList(); }
    void         sync()                                          { KGlobal::config()->sync(); }
    #else
    QString      get(const QString &key, const QString &def)     { return contains(key) ? value(key).toString() : def; }
    QStringList  get(const QString &key, const QStringList &def) { return contains(key) ? value(key).toStringList() : def; }
    bool         get(const QString &key, bool def)               { return contains(key) ? value(key).toBool() : def; }
    int          get(const QString &key, int def)                { return contains(key) ? value(key).toInt() : def; }
    unsigned int get(const QString &key, unsigned int def)       { return contains(key) ? value(key).toUInt() : def; }
    QByteArray   get(const QString &key, const QByteArray &def)  { return contains(key) ? value(key).toByteArray() : def; }
    QSize        get(const QString &key, const QSize &def)       { return contains(key) ? value(key).toSize() : def; }
    QDateTime    get(const QString &key, const QDateTime &def)   { return contains(key) ? value(key).toDateTime() : def; }
    void         set(const QString &key, const QString &val)     { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, const char *val)        { if (!hasEntry(key) || get(key, QLatin1String(val))!=QLatin1String(val)) setValue(key, val); }
    void         set(const QString &key, const QStringList &val) { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, bool val)               { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, int val)                { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, unsigned int val)       { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, const QByteArray &val)  { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, const QSize &val)       { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    void         set(const QString &key, const QDateTime &val)   { if (!hasEntry(key) || get(key, val)!=val) setValue(key, val); }
    bool         hasGroup(const QString &grp)                    { return -1!=childGroups().indexOf(grp); }
    void         removeGroup(const QString &grp)                 { remove(grp); }
    bool         hasEntry(const QString &key)                    { return contains(key); }
    void         removeEntry(const QString &key)                 { remove(key); }
    #endif
    int          get(const QString &key, int def, int min, int max);
    QString      getPath(const QString &key, const QString &def);
    void         setPath(const QString &key, const QString &val) { return set(key, Utils::homeToTilda(val)); }

private:
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup topCfg;
    KConfigGroup *cfg;
    #endif
};

#endif
