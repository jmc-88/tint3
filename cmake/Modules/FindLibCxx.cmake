# Check that the compiler accepts the flag "-stdlib=libc++", and
# can successfully compile, link and execute a simple test binary
# using it.
#
# Exports:
#  LIBCXX_FOUND - Tells if the library was found on the system.
#  LIBCXX_LIBRARIES - If the library was found, this is a list of
#      paths to the library files.

option(TINT3_ENABLE_LIBCXX
       "Whether to force the use of libc++, if found"
       ON)

mark_as_advanced(LIBCXX_FOUND)
mark_as_advanced(LIBCXX_LIBRARIES)

message(STATUS "Looking for libc++")
find_library(
  LIBCXX_LIBRARIES
  NAMES c++)

if(LIBCXX_LIBRARIES)
  set(__LIBCXX_COMPILE_TEST_SOURCE
      "#include <iostream>
      #include <string>

      int main() {
        std::string s = \"test\";
        std::cout << s << '\n';
      }")
  set(__ORIGINAL_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  set(__ORIGINAL_CMAKE_REQUIRED_QUIET "${CMAKE_REQUIRED_QUIET}")

  # This uses MATCHES instead of STREQUAL to accept both Clang and AppleClang.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # If using Clang, it's enough to provide the "-stdlib=libc++" flag.
    # Check if that has the effect of successfully compiling and linking a
    # simple test program.

    include(CheckCXXSourceRuns)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_runs(
      "${__LIBCXX_COMPILE_TEST_SOURCE}"
      __LIBCXX_COMPILES)

    set(LIBCXX_FOUND "${__LIBCXX_COMPILES}")
    unset(__LIBCXX_COMPILES)

    if(LIBCXX_FOUND AND TINT3_ENABLE_LIBCXX)
      set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -stdlib=libc++")
      set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
      set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++")
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # If using GCC, we need to apply more tweaks to the compilation flags in
    # order to link to libc++ instead of libstdc++.
    # See: https://libcxx.llvm.org/docs/UsingLibcxx.html#using-libc-with-gcc
    #
    # TODO: the /usr prefix should not be hardcoded

    include(CheckCXXSourceRuns)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -nostdinc++ -I/usr/include/c++/v1 -nodefaultlibs -lc -lm -lgcc -lgcc_s -lc++ -lc++abi")
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_runs(
      "${__LIBCXX_COMPILE_TEST_SOURCE}"
      __LIBCXX_COMPILES)

    set(LIBCXX_FOUND "${__LIBCXX_COMPILES}")
    unset(__LIBCXX_COMPILES)

    if(LIBCXX_FOUND AND TINT3_ENABLE_LIBCXX)
      set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -nostdinc++ -I/usr/include/c++/v1")
      set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -nodefaultlibs")
      set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -nodefaultlibs")
      link_libraries(-lc -lm -lgcc -lgcc_s -lc++ -lc++abi)
    endif()

    unset(__LIBCXX_COMPILES)
  else()
    message(FATAL_ERROR "Unsupported C++ compiler ${CMAKE_CXX_COMPILER_ID}")
  endif()

  set(CMAKE_REQUIRED_QUIET "${__ORIGINAL_CMAKE_REQUIRED_QUIET}")
  set(CMAKE_REQUIRED_FLAGS "${__ORIGINAL_CMAKE_REQUIRED_FLAGS}")

  unset(__ORIGINAL_CMAKE_REQUIRED_QUIET)
  unset(__ORIGINAL_CMAKE_REQUIRED_FLAGS)
  unset(__LIBCXX_COMPILE_TEST_SOURCE)
endif()

if(LIBCXX_FOUND)
  message(STATUS "Looking for libc++ - found at ${LIBCXX_LIBRARIES}")
else()
  message(STATUS "Looking for libc++ - NOT found")
endif()
