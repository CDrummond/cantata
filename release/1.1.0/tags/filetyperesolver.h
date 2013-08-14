/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/****************************************************************************************
 * Copyright (c) 2005 Martin Aumueller <aumueller@reserv.at>                            *
 * Copyright (c) 2011 Ralf Engels <ralf-engels@gmx.de>                                  *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef AMAROK_FILETYPERESOLVER_H
#define AMAROK_FILETYPERESOLVER_H

#include <taglib/fileref.h>

namespace Meta
{
namespace Tag
{
class FileTypeResolver : public TagLib::FileRef::FileTypeResolver
{
    TagLib::File *createFile(TagLib::FileName fileName,
                             bool readAudioProperties,
                             TagLib::AudioProperties::ReadStyle audioPropertiesStyle) const;

public:
    virtual ~FileTypeResolver() {}
};
}
}

#endif
