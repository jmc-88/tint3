#ifndef TINT3_CONFIG_HH
#define TINT3_CONFIG_HH

#include <string>

#include "server.hh"
#include "parser/parser.hh"

namespace test {

class ConfigReader;

}  // namespace test

namespace config {

enum Tokens {
  kNewLine = (parser::kEOF + 1),
  kPoundSign,
  kEqualsSign,
  kIdentifier,
  kWhitespace,
  kAny,
};

extern const parser::Lexer kLexer;

void ExtractValues(const std::string& value, std::string& v1, std::string& v2,
                   std::string& v3);

class Parser;
class Reader {
 public:
  Reader(Server* server, bool snapshot_mode);

  bool LoadFromFile(std::string const& path);
  bool LoadFromDefaults();

 private:
  friend class Parser;
  friend class test::ConfigReader;

  Server* server_;
  bool new_config_file_;
  bool snapshot_mode_;

  void AddEntry(std::string const& key, std::string const& value);
  int GetMonitor(std::string const& monitor_name) const;
};

class Parser : public parser::ParseCallback {
  //  config_entry  ::= '\n' config_entry
  //                    | comment '\n' config_entry
  //                    | assignment;
  //        comment ::= '#' value '\n';
  //     assignment ::= \s* identifier \s+ '=' \s+ value \s* '\n';
  //     identifier ::= [A-Za-z][A-Za-z0-9-]*
  //          value ::= [^\n]+

 public:
  explicit Parser(Reader* reader);

  bool operator()(parser::TokenList* tokens);

 private:
  Reader* reader_;

  bool ConfigEntryParser(parser::TokenList* tokens);
  bool Comment(parser::TokenList* tokens);
  bool Assignment(parser::TokenList* tokens);
  bool AddKeyValue(std::string key, std::string value);
};

}  // namespace config

#endif  // TINT3_CONFIG_HH
