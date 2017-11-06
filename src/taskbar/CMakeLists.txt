add_library(
  task_lib STATIC
  task.cc)

target_include_directories(
  task_lib
  PRIVATE
    ${IMLIB2_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  task_lib
  PRIVATE
    log_lib
    panel_lib
    server_lib
    taskbar_lib
    tooltip_lib
    window_lib
  PUBLIC
    area_lib
    common_lib
    pango_lib
    timer_lib
    ${IMLIB2_LIBRARIES}
    ${X11_X11_LIB})

add_library(
  taskbar_lib STATIC
  taskbar.cc)

target_include_directories(
  taskbar_lib
  PRIVATE
    ${IMLIB2_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  taskbar_lib
  PRIVATE
    log_lib
    panel_lib
    server_lib
    tooltip_lib
    window_lib
  PUBLIC
    task_lib
    taskbarbase_lib
    taskbarname_lib
    ${X11_X11_LIB})

add_library(
  taskbarbase_lib STATIC
  taskbarbase.cc)

target_link_libraries(
  taskbarbase_lib
  PRIVATE
    server_lib
  PUBLIC
    area_lib)

add_library(
  taskbarname_lib STATIC
  taskbarname.cc)

target_include_directories(
  taskbarname_lib
  PRIVATE
    ${IMLIB2_INCLUDE_DIRS}
    ${X11_X11_INCLUDE_DIRS})

target_link_libraries(
  taskbarname_lib
  PRIVATE
    panel_lib
    server_lib
    taskbar_lib
    window_lib
  PUBLIC
    area_lib
    common_lib
    pango_lib
    taskbarbase_lib)
