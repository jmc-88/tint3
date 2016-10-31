#ifndef TINT3_CONFIG_H
#define TINT3_CONFIG_H

#include <string>

#include "server.h"

extern std::string config_path;
extern std::string snapshot_path;

namespace test {

class ConfigReader;

}  // namespace test

namespace config {

void ExtractValues(const std::string& value, std::string& v1, std::string& v2,
                   std::string& v3);

class Reader {
 public:
  explicit Reader(Server* server);

  bool LoadFromFile(std::string const& path);
  bool LoadFromDefaults();

 private:
  friend class test::ConfigReader;

  Server* server_;
  bool new_config_file_;

  bool ParseLine(std::string const& line, std::string& key,
                 std::string& value) const;
  void AddEntry(std::string const& key, std::string const& value);
  int GetMonitor(std::string const& monitor_name) const;
};

}  // namespace config

#endif  // TINT3_CONFIG_H
