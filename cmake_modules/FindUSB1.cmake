# - Try to find libopenUSB1
# Once done this will define
#
#  USB1_FOUND - system has libopenUSB1
#  USB1_INCLUDE_DIRS - the libopenUSB1 include directories
#  USB1_LIBRARIES - Link these to use libopenUSB1
#  USB1_DEFINITIONS - Compiler switches required for using libopenUSB1
#
#  USB1_HAS_USB1_VERSION - defined when libopenUSB1 has USB1_get_version()

#=============================================================================
# Copyright (c) 2022 Ilia Platone <info@iliaplatone.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_path(USB1_INCLUDE_DIR
  NAMES
    libusb.h
  PATH_SUFFIXES
    libusb-1.0
)

find_library(USB1_LIBRARY
  NAMES
    usb-1.0
)

set(USB1_INCLUDE_DIRS ${USB1_INCLUDE_DIR})
set(USB1_LIBRARIES ${USB1_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(USB1
  FOUND_VAR
    USB1_FOUND
  REQUIRED_VARS
    USB1_LIBRARIES
    USB1_INCLUDE_DIRS
  VERSION_VAR
    USB1_VERSION
)

mark_as_advanced(USB1_INCLUDE_DIRS USB1_LIBRARIES)

if(USB1_FOUND)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${USB1_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${USB1_LIBRARIES})
  check_cxx_source_compiles("#include <USB1.h>
    int main() { USB1_get_version(); return 0; }" USB1_HAS_USB1_VERSION)
  cmake_pop_check_state()
endif()
