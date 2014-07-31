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
void default_config();

bool config_read_file(std::string const& path);
bool config_read();

#endif

