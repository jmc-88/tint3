/**************************************************************************
* config :
* - parse config file in Panel struct.
*
* Check COPYING file for Copyright
*
**************************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#include <string>

extern std::string config_path;
extern std::string snapshot_path;

// default global data
void DefaultConfig();

namespace config {

void ExtractValues(const std::string& value, std::string& v1, std::string& v2,
                   std::string& v3);
bool ReadFile(std::string const& path);
bool Read();

}  // namespace config

#endif
