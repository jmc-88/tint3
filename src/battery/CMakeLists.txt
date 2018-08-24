add_library(
  battery_interface_lib INTERFACE)

target_sources(
  battery_interface_lib
  INTERFACE
    "${PROJECT_SOURCE_DIR}/src/battery/battery_interface.hh")

add_library(
  battery_lib STATIC
  battery.cc)

target_link_libraries(
  battery_lib
  PRIVATE
    common_lib
    fs_lib
    log_lib
    panel_lib
    server_lib
    subprocess_lib
    window_lib
    absl::str_format
    ${CAIRO_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES}
  PUBLIC
    area_lib
    battery_interface_lib
    common_lib
    pango_lib
    timer_lib)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(
    battery_lib
    PRIVATE
      linux_sysfs_lib)

  add_library(
    linux_sysfs_lib STATIC
    linux_sysfs.cc)

  target_link_libraries(
    linux_sysfs_lib
    PRIVATE
      common_lib
      fs_lib
    PUBLIC
      battery_interface_lib)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_link_libraries(
    battery_lib
    PRIVATE
      freebsd_acpiio_lib)

  add_library(
    freebsd_acpiio_lib STATIC
    freebsd_acpiio.cc)

  target_link_libraries(
    freebsd_acpiio_lib
    PRIVATE
      log_lib
    PUBLIC
      battery_interface_lib)
endif()
