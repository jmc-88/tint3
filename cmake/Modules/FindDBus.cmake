# tint3 - A lightweight panel for X11.
# Copyright (C) 2015  Daniele Cocca <daniele.cocca@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# - Try to find DBus
# Once done this will define
#  DBUS_FOUND - System has DBus
#  DBUS_INCLUDE_DIRS - The DBus include directories
#  DBUS_LIBRARIES - The libraries needed to use DBus
#  DBUS_DEFINITIONS - Compiler switches required for using DBus

find_package(PkgConfig)
pkg_check_modules(PC_DBUS QUIET dbus-1)
set(DBUS_DEFINITIONS ${PC_DBUS_CFLAGS_OTHER})

find_path(DBUS_INCLUDE_DIR NAMES dbus/dbus.h
          HINTS ${PC_DBUS_INCLUDEDIR} ${PC_DBUS_INCLUDE_DIRS}
          PATH_SUFFIXES dbus-1.0)

find_path(DBUS_ARCH_INCLUDE_DIR NAMES dbus/dbus-arch-deps.h
          HINTS ${PC_DBUS_INCLUDEDIR} ${PC_DBUS_INCLUDE_DIRS}
          PATH_SUFFIXES include)

find_library(DBUS_LIBRARY NAMES dbus-1
             HINTS ${PC_DBUS_LIBDIR} ${PC_DBUS_LIBRARY_DIRS})

set(DBUS_LIBRARIES ${DBUS_LIBRARY})
set(DBUS_INCLUDE_DIRS ${DBUS_INCLUDE_DIR} ${DBUS_ARCH_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DBUS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DBus  DEFAULT_MSG
                                  DBUS_LIBRARY DBUS_INCLUDE_DIR)
mark_as_advanced(DBUS_INCLUDE_DIR DBUS_LIBRARY)
