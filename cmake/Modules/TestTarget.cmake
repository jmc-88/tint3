option(TINT3_ENABLE_ASAN_FOR_TESTS
       "Whether to use AddressSanitizer for test binaries"
       ON)

function(test_target target_name)
  add_executable(${target_name} ${ARGN})
  add_test(NAME ${target_name} COMMAND ${target_name})

  if(${TINT3_ENABLE_ASAN_FOR_TESTS})
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      # When using Clang, turn on AddressSanitizer by default for all
      # the test targets.
      set_target_properties(${target_name} PROPERTIES
        LINK_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    endif()
  endif()
endfunction(test_target)
