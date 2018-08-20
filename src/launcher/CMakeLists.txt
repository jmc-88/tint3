add_library(
  launcher_lib STATIC
  launcher.cc)

target_include_directories(
  launcher_lib
  PRIVATE
    ${CAIRO_INCLUDE_DIRS})

target_link_libraries(
  launcher_lib
  PRIVATE
    desktop_entry_lib
    fs_lib
    log_lib
    panel_lib
    server_lib
    startup_notification_lib
    subprocess_lib
    taskbar_lib
    xdg_lib
    ${CAIRO_LIBRARIES}
  PUBLIC
    area_lib
    common_lib
    imlib2_lib
    xsettings_client_lib)

test_target(
  launcher_test
  SOURCES
    launcher_test.cc
  DEPENDS
    testdata
  LINK_LIBRARIES
    environment_lib
    launcher_lib
    panel_lib
    testmain)

add_library(
  desktop_entry_lib STATIC
  desktop_entry.cc)

target_link_libraries(
  desktop_entry_lib
  PRIVATE
    common_lib
    log_lib
    absl::strings
  PUBLIC
    parser_lib
    absl::variant)

test_target(
  desktop_entry_test
  SOURCES
    desktop_entry_test.cc
  LINK_LIBRARIES
    desktop_entry_lib
    parser_lib
    testmain)

add_library(
  xsettings_client_lib STATIC
  xsettings-client.cc)

target_sources(
  xsettings_client_lib
  PRIVATE
    xsettings-common.cc)

target_include_directories(
  xsettings_client_lib
  PRIVATE
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  xsettings_client_lib
  PRIVATE
    log_lib
    panel_lib
    server_lib
    x11_lib
  PUBLIC
    server_lib
    ${X11_X11_LIB})
