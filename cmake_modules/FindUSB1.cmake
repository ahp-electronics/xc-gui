# - Try to find the freetype library
# Once done this defines
#
#  USB1_FOUND - system has USB1
#  USB1_INCLUDE_DIR - the USB1 include directory
#  USB1_LIBRARIES - Link these to use USB1

# Copyright (c) 2006, 2008  Laurent Montel, <montel@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


if (USB1_INCLUDE_DIR AND USB1_LIBRARIES)

  # in cache already
  set(USB1_FOUND TRUE)

else (USB1_INCLUDE_DIR AND USB1_LIBRARIES)
  IF (NOT WIN32)
    # use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PC_LIBUSB libusb-1.0)
  ENDIF(NOT WIN32)

  FIND_PATH(USB1_INCLUDE_DIR libusb.h
    PATHS ${PC_LIBUSB_INCLUDEDIR} ${PC_LIBUSB_INCLUDE_DIRS})

  FIND_LIBRARY(USB1_LIBRARIES NAMES usb-1.0
    PATHS ${PC_LIBUSB_LIBDIR} ${PC_LIBUSB_LIBRARY_DIRS})

  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(USB1 DEFAULT_MSG USB1_LIBRARIES USB1_INCLUDE_DIR)

  MARK_AS_ADVANCED(USB1_INCLUDE_DIR USB1_LIBRARIES)

endif (USB1_INCLUDE_DIR AND USB1_LIBRARIES)
