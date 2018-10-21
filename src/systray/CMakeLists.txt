add_library(
  systraybar_lib STATIC
  systraybar.cc)

target_link_libraries(
  systraybar_lib
  PRIVATE
    collection_lib
    log_lib
    panel_lib
    server_lib
    x11_lib
    absl::strings
    ${IMLIB2_LIBRARIES}
    ${X11_Xcomposite_LIB}
    ${X11_Xrender_LIB}
  PUBLIC
    area_lib
    common_lib
    timer_lib
    tray_window_lib
    ${X11_Xdamage_LIB})

add_library(
  tray_window_lib STATIC
  tray_window.cc)

target_include_directories(
  tray_window_lib
  PUBLIC
    ${X11_X11_INCLUDE_DIRS}
    ${X11_Xdamage_INCLUDE_DIRS})

target_link_libraries(
  tray_window_lib
  PRIVATE
    server_lib
  PUBLIC
    timer_lib
    ${X11_X11_LIB}
    ${X11_Xdamage_LIB})
