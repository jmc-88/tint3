include(CMakeParseArguments)
option(TINT3_ENABLE_ASAN_FOR_TESTS
       "Whether to use AddressSanitizer for test binaries"
       ON)

function(test_target target_name)
  set(options USE_XVFB_RUN)
  set(multiValueArgs SOURCES DEPENDS LINK_LIBRARIES)
  cmake_parse_arguments(TEST_TARGET "${options}" "" "${multiValueArgs}" ${ARGN})

  add_executable(${target_name} ${TEST_TARGET_SOURCES})
  if(TEST_TARGET_USE_XVFB_RUN)
    add_test(
      NAME ${target_name}
      COMMAND "${CMAKE_SOURCE_DIR}/test/xvfb-run.sh" "${CMAKE_BINARY_DIR}/${target_name}")
  else()
    add_test(NAME ${target_name} COMMAND ${target_name})
  endif()

  if(TEST_TARGET_LINK_LIBRARIES)
    target_link_libraries(${target_name} ${TEST_TARGET_LINK_LIBRARIES})
  endif()

  if(TEST_TARGET_DEPENDS)
    add_dependencies(${target_name} ${TEST_TARGET_DEPENDS})
  endif()

  if(${TINT3_ENABLE_ASAN_FOR_TESTS})
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      # When using Clang, turn on AddressSanitizer by default for all
      # the test targets.
      set_target_properties(${target_name} PROPERTIES
        LINK_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    endif()
  endif()
endfunction(test_target)
