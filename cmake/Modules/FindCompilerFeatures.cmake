include(CheckCXXSymbolExists)

check_cxx_symbol_exists(std::stoi string HAVE_STD_STOI)
check_cxx_symbol_exists(std::stol string HAVE_STD_STOL)
check_cxx_symbol_exists(std::stof string HAVE_STD_STOF)
