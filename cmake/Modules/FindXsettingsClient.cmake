# Check that the xsettings-client package is available on the system.
#
# Exports:
#  XSETTINGS_CLIENT_FOUND - Tells if the package was found on the system.
#  XSETTINGS_CLIENT_LIBRARIES - If the package was found, this is a list of
#      compiler flags to link to the relevant libraries.

include(FindPkgConfig)
pkg_check_modules(XSETTINGS_CLIENT QUIET libxsettings-client)

# Very basic fallback for the case where pkg-config doesn't find the package.
# This could happen even if the xsettings-client library is in fact available,
# as some systems (e.g. Arch Linux) don't ship with libxsettings-client.pc.
# See also: https://github.com/jmc-88/tint3/issues/40
if(NOT XSETTINGS_CLIENT_FOUND)
  mark_as_advanced(FORCE CLEAR XSETTINGS_CLIENT_FOUND)
  mark_as_advanced(FORCE CLEAR XSETTINGS_CLIENT_LIBRARIES)

  find_library(
    XSETTINGS_CLIENT_LIBRARIES
    NAMES Xsettings-client)
  if(NOT XSETTINGS_CLIENT_LIBRARIES)
    message(FATAL_ERROR "libxsettings-client couldn't be found")
  endif()

  # If we arrived here successfully, then all checks have succeeded and we
  # assume the library was found.
  set(XSETTINGS_CLIENT_FOUND TRUE)
endif()