# - Try to find UBX
# Once done this will define
#
#  UBX_FOUND - system has UBX
#  UBX_INCLUDE_DIR - the UBX include directory
#  UBX_LIBRARIES - Link these to use UBX
#  UBX_VERSION_STRING - Human readable version number of ahp_gt
#  UBX_VERSION_MAJOR  - Major version number of ahp_gt
#  UBX_VERSION_MINOR  - Minor version number of ahp_gt

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (UBX_INCLUDE_DIR AND UBX_LIBRARIES)

  # in cache already
  set(UBX_FOUND TRUE)
  message(STATUS "Found UBX: ${UBX_LIBRARIES}")


else (UBX_INCLUDE_DIR AND UBX_LIBRARIES)

    find_path(UBX_INCLUDE_DIR u_gnss.h
      PATH_SUFFIXES ubx
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
    )

  find_library(UBX_LIBRARIES NAMES ubx
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

if(UBX_INCLUDE_DIR AND UBX_LIBRARIES)
  set(UBX_FOUND TRUE)
else (UBX_INCLUDE_DIR AND UBX_LIBRARIES)
  set(UBX_FOUND FALSE)
endif(UBX_INCLUDE_DIR AND UBX_LIBRARIES)

  if (UBX_FOUND)
    if (NOT UBX_FIND_QUIETLY)
      message(STATUS "Found UBX: ${UBX_LIBRARIES}")
    endif (NOT UBX_FIND_QUIETLY)
  else (UBX_FOUND)
    if (UBX_FIND_REQUIRED)
      message(FATAL_ERROR "UBX not found. Please install libubx-dev")
    endif (UBX_FIND_REQUIRED)
  endif (UBX_FOUND)

  mark_as_advanced(UBX_LIBRARIES)
  
endif (UBX_INCLUDE_DIR AND UBX_LIBRARIES)
