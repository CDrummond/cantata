/***************************************************************************
 *   Copyright (C) 2007 by Kevin Krammer <kevin.krammer@gmx.at>            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

/*
 * This file was copied to solid from [akonadi]/libs/xdgbasedirs.cpp and slimmed down.
 * See xdgbasedirs_p.h for notes about slimming down.
 */

#include "xdgbasedirs_p.h"

#include <QDir>
#include <QFileInfo>

static QStringList splitPathList( const QString &pathList )
{
  return pathList.split( QLatin1Char( ':' ) );
}

class XdgBaseDirsSingleton
{
  public:
    QString homePath( const char *variable, const char *defaultSubDir );

    QStringList systemPathList( const char *variable, const char *defaultDirList );

  public:
    QString mConfigHome;
    QString mDataHome;

    QStringList mConfigDirs;
    QStringList mDataDirs;
};

Q_GLOBAL_STATIC( XdgBaseDirsSingleton, instance )

QString Solid::XdgBaseDirs::homePath( const char *resource )
{
  if ( qstrncmp( "data", resource, 4 ) == 0 ) {
    if ( instance()->mDataHome.isEmpty() ) {
      instance()->mDataHome = instance()->homePath( "XDG_DATA_HOME", ".local/share" );
    }
    return instance()->mDataHome;
  } else if ( qstrncmp( "config", resource, 6 ) == 0 ) {
    if ( instance()->mConfigHome.isEmpty() ) {
      instance()->mConfigHome = instance()->homePath( "XDG_CONFIG_HOME", ".config" );
    }
    return instance()->mConfigHome;
  }

  return QString();
}

QStringList Solid::XdgBaseDirs::systemPathList( const char *resource )
{
  if ( qstrncmp( "data", resource, 4 ) == 0 ) {
    if ( instance()->mDataDirs.isEmpty() ) {
      instance()->mDataDirs = instance()->systemPathList( "XDG_DATA_DIRS", "/usr/local/share:/usr/share" );
    }
    return instance()->mDataDirs;
  } else if ( qstrncmp( "config", resource, 6 ) == 0 ) {
    if ( instance()->mConfigDirs.isEmpty() ) {
      instance()->mConfigDirs = instance()->systemPathList( "XDG_CONFIG_DIRS", "/etc/xdg" );
    }
    return instance()->mConfigDirs;
  }

  return QStringList();
}

QString Solid::XdgBaseDirs::findResourceFile( const char *resource, const QString &relPath )
{
  const QString fullPath = homePath( resource ) + QLatin1Char( '/' ) + relPath;

  QFileInfo fileInfo( fullPath );
  if ( fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable() ) {
    return fullPath;
  }

  const QStringList pathList = systemPathList( resource );

  foreach ( const QString &path, pathList ) {
    fileInfo = QFileInfo( path + QLatin1Char('/' ) + relPath );
    if ( fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable() ) {
      return fileInfo.absoluteFilePath();
    }
  }

  return QString();
}

QString XdgBaseDirsSingleton::homePath( const char *variable, const char *defaultSubDir )
{
  const QByteArray env = qgetenv( variable );

  QString xdgPath;
  if ( env.isEmpty() ) {
    xdgPath = QDir::homePath() + QLatin1Char( '/' ) + QLatin1String( defaultSubDir );
  } else if ( env.startsWith( '/' ) ) {
    xdgPath = QString::fromLocal8Bit( env );
  } else {
    xdgPath = QDir::homePath() + QLatin1Char( '/' ) + QString::fromLocal8Bit( env );
  }

  return xdgPath;
}

QStringList XdgBaseDirsSingleton::systemPathList( const char *variable, const char *defaultDirList )
{
  const QByteArray env = qgetenv( variable );

  QString xdgDirList;
  if ( env.isEmpty() ) {
    xdgDirList = QLatin1String( defaultDirList );
  } else {
    xdgDirList = QString::fromLocal8Bit( env );
  }

  return splitPathList( xdgDirList );
}
