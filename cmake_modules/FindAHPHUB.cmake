# - Try to find AHPHUB
# Once done this will define
#
#  AHPHUB_FOUND - system has AHPHUB
#  AHPHUB_INCLUDE_DIR - the AHPHUB include directory
#  AHPHUB_LIBRARIES - Link these to use AHPHUB
#  AHPHUB_VERSION_STRING - Human readable version number of ahp_hub
#  AHPHUB_VERSION_MAJOR  - Major version number of ahp_hub
#  AHPHUB_VERSION_MINOR  - Minor version number of ahp_hub

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)

  # in cache already
  set(AHPHUB_FOUND TRUE)
  message(STATUS "Found AHPHUB: ${AHPHUB_LIBRARIES}")


else (AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)

    find_path(AHPHUB_INCLUDE_DIR ahp_hub.h
      PATH_SUFFIXES ahp
      ${_obIncDir}
      ${GNUWIN32_DIR}/include
    )

  find_library(AHPHUB_LIBRARIES NAMES ahp_hub
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
    HINTS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
  )

if(AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)
  set(AHPHUB_FOUND TRUE)
else (AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)
  set(AHPHUB_FOUND FALSE)
endif(AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)

  if (AHPHUB_FOUND)
    if (NOT AHPHUB_FIND_QUIETLY)
      message(STATUS "Found AHPHUB: ${AHPHUB_LIBRARIES}")
    endif (NOT AHPHUB_FIND_QUIETLY)
  else (AHPHUB_FOUND)
    if (AHPHUB_FIND_REQUIRED)
      message(FATAL_ERROR "AHPHUB not found. Please install libahp_hub-dev")
    endif (AHPHUB_FIND_REQUIRED)
  endif (AHPHUB_FOUND)

  mark_as_advanced(AHPHUB_LIBRARIES)
  
endif (AHPHUB_INCLUDE_DIR AND AHPHUB_LIBRARIES)
