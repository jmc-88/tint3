include(CMakeParseArguments)
option(TINT3_ENABLE_ASAN_FOR_TESTS
       "Whether to use AddressSanitizer for test binaries"
       ON)

set(LSAN_OPTIONS "report_objects=1:suppressions=${CMAKE_SOURCE_DIR}/test/suppressions/lsan.txt")
set(ASAN_OPTIONS "allow_addr2line=1:fast_unwind_on_malloc=0")
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  string(APPEND ASAN_OPTIONS ":malloc_context_size=2")
endif()

function(test_target target_name)
  set(options USE_XVFB_RUN)
  set(multiValueArgs SOURCES DEPENDS INCLUDE_DIRS LINK_LIBRARIES)
  cmake_parse_arguments(TEST_TARGET "${options}" "" "${multiValueArgs}" ${ARGN})

  add_executable(${target_name} ${TEST_TARGET_SOURCES})
  if(TEST_TARGET_USE_XVFB_RUN)
    add_test(
      NAME
        ${target_name}
      COMMAND
        "${CMAKE_SOURCE_DIR}/test/xvfb-run.sh" $<TARGET_FILE:${target_name}>
      WORKING_DIRECTORY
        "${CMAKE_BINARY_DIR}")
  else()
    add_test(
      NAME
        ${target_name}
      COMMAND
        $<TARGET_FILE:${target_name}>
      WORKING_DIRECTORY
        "${CMAKE_BINARY_DIR}")
  endif()

  if(TEST_TARGET_INCLUDE_DIRS)
    target_include_directories(${target_name} ${TEST_TARGET_INCLUDE_DIRS})
  endif()

  if(TEST_TARGET_LINK_LIBRARIES)
    target_link_libraries(${target_name} ${TEST_TARGET_LINK_LIBRARIES})
  endif()

  if(TEST_TARGET_DEPENDS)
    add_dependencies(${target_name} ${TEST_TARGET_DEPENDS})
  endif()

  if(${TINT3_ENABLE_ASAN_FOR_TESTS})
    # When requested, turn on AddressSanitizer for test targets.
    set_target_properties(
      ${target_name} PROPERTIES
      LINK_FLAGS
        "-fsanitize=address"
        "-fno-omit-frame-pointer"
        "-fno-optimize-sibling-calls")
    set_tests_properties(
      ${target_name} PROPERTIES
      ENVIRONMENT
        "LSAN_OPTIONS=${LSAN_OPTIONS};ASAN_OPTIONS=${ASAN_OPTIONS}")
  endif()
endfunction(test_target)
