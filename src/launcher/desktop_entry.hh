#ifndef TINT3_LAUNCHER_DESKTOP_ENTRY_HH
#define TINT3_LAUNCHER_DESKTOP_ENTRY_HH

#include <map>
#include <string>
#include <vector>

#include "util/variant.hh"

#include "parser/parser.hh"

namespace launcher {
namespace desktop_entry {

enum Tokens {
  kNewLine = (parser::kEOF + 1),
  kPoundSign,
  kLeftBracket,
  kRightBracket,
  kEqualsSign,
  kIdentifier,
  kWhitespace,
  kAny,
};

extern const parser::Lexer kLexer;

class Group {
public:
  using StringList = std::vector<std::string>;
  using LocaleString = std::map<std::string, std::string>;

  using Value = util::variant::Value<
    std::string,
    StringList,
    LocaleString,
    bool,
    float
  >;

  static const std::string kInvalidName;

  explicit Group(std::string const& name);
  Group(Group const& other);
  Group(Group&& other);

  Group& operator=(Group other);

  std::string const& GetName() const;

  template<typename T>
  T const& GetEntry(std::string const& key) {
    return entries_[key].Get<T>();
  }

  template<typename T>
  void AddEntry(std::string const& key, T const& value) {
    entries_.emplace(key, value);
  }

private:
  std::string name_;
  std::map<std::string, Value> entries_;
};

class Parser : public parser::ParseCallback {
  //  desktop_entry ::= '\n' desktop_entry
  //                    | comment '\n' desktop_entry
  //                    | group;
  //        comment ::= '#' value '\n';
  //          group ::= '[' group_header ']' '\n' group_entry;
  //   group_header ::= [^\[\]]+
  //    group_entry ::= identifier '=' value '\n' group_entry
  //                    | comment '\n' group_entry
  //                    | '\n' group_entry
  //                    | group;
  //     identifier ::= [A-Za-z][A-Za-z0-9-]*
  //          value ::= [^\n]+

public:
  Parser();

  bool operator()(parser::TokenList* tokens);
  std::vector<Group> GetGroups() const;

private:
  Group current_group_;
  std::vector<Group> groups_;

  bool DesktopEntry(parser::TokenList* tokens);
  bool Comment(parser::TokenList* tokens);
  bool GroupParser(parser::TokenList* tokens);
  bool GroupHeader(parser::TokenList* tokens);
  bool GroupEntry(parser::TokenList* tokens);
};

}  // namespace desktop_entry
}  // namespace launcher

#endif  // TINT3_LAUNCHER_DESKTOP_ENTRY_HH
