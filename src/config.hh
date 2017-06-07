#ifndef TINT3_CONFIG_HH
#define TINT3_CONFIG_HH

#include <string>

#include "parser/parser.hh"
#include "server.hh"
#include "util/fs.hh"

namespace test {

class ConfigReader;

}  // namespace test

namespace config {

enum Tokens {
  kNewLine = (parser::kEOF + 1),
  kPoundSign,
  kEqualsSign,
  kImport,
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
  Reader(Server* server);

  virtual bool LoadFromFile(std::string const& path);
  bool LoadFromDefaults();

 private:
  friend class Parser;
  friend class test::ConfigReader;

  Server* server_;
  bool new_config_file_;

  void AddEntry(std::string const& key, std::string const& value);
  unsigned int GetMonitor(std::string const& monitor_name) const;
};

class Parser : public parser::ParseCallback {
  //  config_entry  ::= '\n' config_entry
  //                    | comment '\n' config_entry
  //                    | import
  //                    | assignment;
  //        comment ::= '#' value '\n';
  //     assignment ::= \s* identifier \s+ '=' \s+ value \s* '\n';
  //         import ::= '@import "' value '"'";
  //     identifier ::= [A-Za-z][A-Za-z0-9-]*;
  //          value ::= [^\n]+;

 public:
  Parser(Reader* reader, std::string const& path);

  bool operator()(parser::TokenList* tokens);

 private:
  Reader* reader_;
  util::fs::Path current_config_path_;

  bool ConfigEntryParser(parser::TokenList* tokens);
  bool Comment(parser::TokenList* tokens);
  bool Import(parser::TokenList* tokens);
  bool Assignment(parser::TokenList* tokens);
  bool AddKeyValue(std::string key, std::string value);
};

}  // namespace config

#endif  // TINT3_CONFIG_HH
