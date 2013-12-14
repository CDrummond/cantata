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
 * This file was copied to solid from [akonadi]/libs/xdgbasedirs_p.h and slimmed down.
 * UDev::PortableMediaPlayer uses findResourceFile() which in turn uses homePath() and
 * systemPathList() so these are exported here too
 */

#ifndef SOLID_XDGBASEDIRS_H
#define SOLID_XDGBASEDIRS_H

class QString;
class QStringList;

namespace Solid
{
    /**
     @brief Resource type based handling of standard directories

     Developers of several Free Software desktop projects have created
     a specification for handling so-called "base directories", i.e.
     lists of system wide directories and directories within each user's
     home directory for installing and later finding certain files.

     This class handles the respective behaviour, i.e. environment variables
     and their defaults, for the following type of resources:
     - "config"
     - "data"

     @author Kevin Krammer, <kevin.krammer@gmx.at>

     @see http://www.freedesktop.org/wiki/Specifications/basedir-spec
     */
    namespace XdgBaseDirs
    {
        /**
         @brief Returns the user specific directory for the given resource type

         Unless the user's environment has a specific path set as an override
         this will be the default as defined in the freedesktop.org base-dir-spec

         @note Caches the value of the first call

         @param resource a named resource type, e.g. "config"

         @return a directory path

         @see systemPathList()
         */
        QString homePath( const char *resource );

        /**
         @brief Returns the list of system wide directories for a given resource type

         The returned list can contain one or more directory paths. If there are more
         than one, the list is sorted by falling priority, i.e. if an entry is valid
         for the respective use case (e.g. contains a file the application looks for)
         the list should not be processed further.

         @note The user's resource path should, to be compliant with the spec,
               always be treated as having higher priority than any path in the
               list of system wide paths

         @note Caches the value of the first call

         @param resource a named resource type, e.g. "config"

         @return a priority sorted list of directory paths

         @see homePath()
         */
        QStringList systemPathList( const char *resource );

        /**
         @brief Searches the resource specific directories for a given file

         Convenience method for finding a given file (with optional relative path)
         in any of the configured base directories for a given resource type.

         Will check the user local directory first and then process the system
         wide path list according to the inherent priority.

         @param resource a named resource type, e.g. "config"
         @param relPath relative path of a file to look for,
                e.g."akonadi/akonadiserverrc"

         @returns the file path of the first match, or @c QString() if no such
                  relative path exists in any of the base directories or if
                  a match is not a file
         */
        QString findResourceFile( const char *resource, const QString &relPath );
    }
}

#endif
