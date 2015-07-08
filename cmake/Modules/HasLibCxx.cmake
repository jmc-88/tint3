# Check that the compiler accepts the flag "-stdlib=libc++", and
# can successfully compile, link and execute a simple test binary
# using it.
#
# Exports:
#  HAS_LIBCXX - Tells if we can successfully link to libc++ or not.

include(CheckCXXSourceRuns)

set(OLD_CMAKE_REQUIRED_FLAGS CMAKE_REQUIRED_FLAGS)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++")

check_cxx_source_runs("int main() {}" HAS_LIBCXX)

set(CMAKE_REQUIRED_FLAGS OLD_CMAKE_REQUIRED_FLAGS)
