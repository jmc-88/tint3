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

  using Value =
      util::variant::Value<std::string, StringList, LocaleString, bool, float>;

  static const std::string kInvalidName;

  explicit Group(std::string const& name);
  Group(Group const& other);
  Group(Group&& other);

  Group& operator=(Group other);

  std::string const& GetName() const;

  bool HasEntry(std::string const& key) const;

  template <typename T>
  bool IsEntry(std::string const& key) {
    return HasEntry(key) && entries_.at(key).Is<T>();
  }

  template <typename T>
  T& GetEntry(std::string const& key) {
    return entries_.at(key).Get<T>();
  }

  template <typename T>
  void AddEntry(std::string const& key, T const& value) {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
      entries_.erase(it);
    }
    entries_.emplace(key, value);
  }

 private:
  std::string name_;
  std::map<std::string, Value> entries_;
};

// A Desktop Entry is a list of groups and their key/value entries.
using DesktopEntry = std::vector<Group>;

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
  DesktopEntry GetDesktopEntry() const;

 private:
  Group current_group_;
  DesktopEntry groups_;

  bool DesktopEntryParser(parser::TokenList* tokens);
  bool Comment(parser::TokenList* tokens);
  bool GroupParser(parser::TokenList* tokens);
  bool GroupHeader(parser::TokenList* tokens);
  bool GroupEntry(parser::TokenList* tokens);
  bool AddKeyValue(std::string key, std::string value, std::string locale);
};

bool ParseBooleanValue(std::string value_string, bool* value_boolean);
bool ParseNumericValue(std::string value_string, float* value_numeric);
bool ParseStringValue(std::string* value_string);
bool ParseStringListValue(std::string value_string,
                          Group::StringList* value_string_list);

bool Validate(DesktopEntry* entry);

std::string BestLocalizedEntry(Group& group, std::string const& key);

}  // namespace desktop_entry
}  // namespace launcher

#endif  // TINT3_LAUNCHER_DESKTOP_ENTRY_HH
