add_subdirectory(clock)
add_subdirectory(dnd)
add_subdirectory(execp)
add_subdirectory(parser)
add_subdirectory(systray)
add_subdirectory(launcher)
add_subdirectory(taskbar)
add_subdirectory(tooltip)
add_subdirectory(util)

if(ENABLE_BATTERY)
  add_subdirectory(battery)
endif()

add_executable(
  tint3
  tint.cc)

target_include_directories(
  tint3
  PRIVATE
    ${X11_Xfixes_INCLUDE_DIRS})

target_link_libraries(
  tint3
  PRIVATE
    config_lib
    dnd_lib
    panel_lib
    server_lib
    subprocess_lib
    startup_notification_lib
    theme_manager_lib
    ${X11_Xfixes_LIB})

install(
  TARGETS tint3
  DESTINATION bin)

add_library(
  config_lib STATIC
  config.cc)

target_include_directories(
  config_lib
  PRIVATE
    ${CAIRO_INCLUDE_DIRS}
    ${IMLIB2_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  config_lib
  PRIVATE
    clock_lib
    launcher_lib
    panel_lib
    systraybar_lib
    task_lib
    taskbar_lib
    taskbarname_lib
    tooltip_lib
    common_lib
    log_lib
    timer_lib
    window_lib
    xdg_lib
    ${CAIRO_LIBRARIES}
    ${IMLIB2_LIBRARIES}
    ${PANGO_LIBRARIES}
    ${X11_X11_LIB}
  PUBLIC
    fs_lib
    parser_lib
    server_lib)

test_target(
  config_test
  SOURCES
    config_test.cc
  LINK_LIBRARIES
    clock_lib
    color_lib
    config_lib
    fs_lib
    fs_test_utils_lib
    gradient_lib
    panel_lib
    server_lib
    testmain
    timer_test_utils_lib
    tooltip_lib)

add_library(
  panel_lib STATIC
  panel.cc)

target_include_directories(
  panel_lib
  PRIVATE
    ${CAIRO_INCLUDE_DIRS}
  PUBLIC
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  panel_lib
  PRIVATE
    config_lib
    log_lib
    server_lib
    tooltip_lib
    x11_lib
    ${CAIRO_LIBRARIES}
  PUBLIC
    area_lib
    color_lib
    common_lib
    clock_lib
    execp_lib
    gradient_lib
    imlib2_lib
    launcher_lib
    systraybar_lib
    task_lib
    taskbar_lib
    timer_lib
    ${X11_X11_LIB})

if(ENABLE_BATTERY)
  target_link_libraries(
    panel_lib
    PUBLIC
      battery_lib)
endif()

test_target(
  panel_test
  SOURCES
    panel_test.cc
  INCLUDE_DIRS
    ${X11_X11_INCLUDE_DIRS}
  LINK_LIBRARIES
    panel_lib
    server_lib
    testmain
    ${X11_X11_LIB})

add_library(
  server_lib STATIC
  server.cc)

target_include_directories(
  server_lib
  PUBLIC
    ${X11_X11_INCLUDE_DIRS}
    ${X11_Xinerama_INCLUDE_DIRS}
  PRIVATE
    ${X11_Xrandr_INCLUDE_DIRS}
    ${X11_Xrender_INCLUDE_DIRS})

target_link_libraries(
  server_lib
  PUBLIC
    config_lib
    common_lib
    log_lib
    startup_notification_lib
    window_lib
    x11_lib
    ${X11_X11_LIB}
    ${X11_Xrandr_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xinerama_LIB})

add_library(
  startup_notification_lib STATIC
  startup_notification.cc)

target_include_directories(
  startup_notification_lib
  PUBLIC
    ${SN_INCLUDE_DIRS})

target_link_libraries(
  startup_notification_lib
  PUBLIC
    ${SN_LIBRARIES})

test_target(
  startup_notification_test
  SOURCES
    startup_notification_test.cc
  INCLUDE_DIRS
    ${X11_X11_INCLUDE_DIRS}
  LINK_LIBRARIES
    common_lib
    environment_lib
    startup_notification_lib
    testmain
    ${X11_X11_LIB}
  USE_XVFB_RUN)

add_library(
  subprocess_lib STATIC
  subprocess.cc)

target_link_libraries(
  subprocess_lib
  PUBLIC
    pipe_lib
  PRIVATE
    log_lib)

test_target(
  subprocess_test
  SOURCES
    subprocess_test.cc
  LINK_LIBRARIES
    subprocess_lib
    testmain)

add_library(
  theme_manager_lib STATIC
  theme_manager.cc)

target_link_libraries(
  theme_manager_lib
  PRIVATE
    common_lib
    log_lib)

if(ENABLE_CURL)
  target_include_directories(
    theme_manager_lib
    PRIVATE
    ${CURL_INCLUDE_DIRS})

  target_link_libraries(
    theme_manager_lib
    PRIVATE
      ${CURL_LIBRARIES})
endif()
