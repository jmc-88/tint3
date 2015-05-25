include(CMakePushCheckState)
cmake_push_check_state()

include(TestCXXAcceptsFlag)
check_cxx_accepts_flag("-std=c++11" CXX_FLAG_CXX11)
check_cxx_accepts_flag("-std=c++0x" CXX_FLAG_CXX0X)
check_cxx_accepts_flag("-std=gnu++11" CXX_FLAG_GNUXX11)
check_cxx_accepts_flag("-std=gnu++0x" CXX_FLAG_GNUXX0X)
if(CXX_FLAG_CXX11)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
elseif(CXX_FLAG_CXX0X)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++0x")
elseif(CXX_FLAG_GNUXX11)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=gnu++11")
elseif(CXX_FLAG_GNUXX0X)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=gnu++0x")
endif()

# Finally, detect features
include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
  "#include <string>
  int main() {
    std::stoi(\"123\");
  }"
  HAVE_STD_STOI)

check_cxx_source_compiles(
  "#include <string>
  int main() {
    std::stol(\"65535\");
  }"
  HAVE_STD_STOL)

check_cxx_source_compiles(
  "#include <string>
  int main() {
    std::stof(\"1.23\");
  }"
  HAVE_STD_STOF)

cmake_pop_check_state()