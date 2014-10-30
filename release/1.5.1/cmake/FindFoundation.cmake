# - Find Foundation on Mac
#
#  FOUNDATION_LIBRARY - the library to use Foundation
#  FOUNDATION_FOUND - true if Foundation has been found

# Copyright (c) 2009, Harald Fernengel <harry@kdevelop.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(CMakeFindFrameworks)

cmake_find_frameworks(Foundation)

if (Foundation_FRAMEWORKS)
   set(FOUNDATION_LIBRARY "-framework Foundation" CACHE FILEPATH "Foundation framework" FORCE)
   set(FOUNDATION_FOUND 1)
endif (Foundation_FRAMEWORKS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Foundation DEFAULT_MSG FOUNDATION_LIBRARY)

