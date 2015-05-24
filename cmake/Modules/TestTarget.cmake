function(test_target target_name)
  add_executable(${target_name} ${ARGN})
  add_test(NAME ${target_name} COMMAND ${target_name})

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # When using Clang, turn on AddressSanitizer by default for all
    # the test targets.
    set_target_properties(${target_name} PROPERTIES
                          LINK_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
  endif()
endfunction(test_target)