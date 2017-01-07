#include <algorithm>
#include <cctype>
#include <clocale>
#include <sstream>
#include <utility>

#include "util/common.hh"
#include "util/log.hh"

#include "launcher/desktop_entry.hh"

namespace launcher {
namespace desktop_entry {

// Whitespace matcher has all the characters accepted by isspace(), except for
// the newline character:
//  http://www.cplusplus.com/reference/cctype/isspace/

const parser::Lexer kLexer{
    std::make_pair('\n', kNewLine),
    std::make_pair("[ \t\v\f\r]+", kWhitespace),
    std::make_pair('#', kPoundSign),
    std::make_pair('[', kLeftBracket),
    std::make_pair(']', kRightBracket),
    std::make_pair('=', kEqualsSign),
    std::make_pair("[A-Za-z][A-Za-z0-9-]*", kIdentifier),
    std::make_pair(".", kAny),
};

const std::string Group::kInvalidName{""};

Group::Group(std::string const& name) : name_(name) {}

Group::Group(Group const& other)
    : name_(other.name_), entries_(other.entries_) {}

Group::Group(Group&& other)
    : name_(std::move(other.name_)), entries_(std::move(other.entries_)) {}

Group& Group::operator=(Group other) {
  std::swap(name_, other.name_);
  std::swap(entries_, other.entries_);
  return *this;
}

std::string const& Group::GetName() const { return name_; }

bool Group::HasEntry(std::string const& key) const {
  return (entries_.find(key) != entries_.end());
}

Parser::Parser() : current_group_(Group::kInvalidName) {}

bool Parser::operator()(parser::TokenList* tokens) {
  return DesktopEntryParser(tokens);
}

DesktopEntry Parser::GetDesktopEntry() const { return groups_; }

bool Parser::DesktopEntryParser(parser::TokenList* tokens) {
  // end of file, stop parsing
  if (tokens->Accept(parser::kEOF)) {
    return true;
  }

  // empty line
  if (tokens->Accept(kNewLine)) {
    return DesktopEntryParser(tokens);
  }

  // comment line
  if (tokens->Current().symbol == kPoundSign) {
    Comment(tokens);
    return DesktopEntryParser(tokens);
  }

  // Group
  if (tokens->Current().symbol == kLeftBracket) {
    return GroupParser(tokens);
  }

  // none of the above, fail parsing
  return false;
}

bool Parser::Comment(parser::TokenList* tokens) {
  // comment, skip entire line
  if (!tokens->Accept(kPoundSign)) {
    return false;
  }
  tokens->SkipUntil(kNewLine);
  return true;
}

bool Parser::GroupParser(parser::TokenList* tokens) {
  if (!GroupHeader(tokens)) {
    return false;
  }
  if (!tokens->Accept(kNewLine)) {
    return false;
  }
  return GroupEntry(tokens);
}

bool Parser::GroupHeader(parser::TokenList* tokens) {
  if (!tokens->Accept(kLeftBracket)) {
    return false;
  }

  std::vector<parser::Token> skipped;
  tokens->SkipUntil(kRightBracket, &skipped);

  if (current_group_.GetName() != Group::kInvalidName) {
    groups_.emplace_back(current_group_);
  }

  current_group_ =
      desktop_entry::Group{parser::TokenList::JoinSkipped(skipped)};

  return tokens->Accept(kRightBracket);
}

bool Parser::GroupEntry(parser::TokenList* tokens) {
  // end of file, stop parsing
  if (tokens->Accept(parser::kEOF)) {
    if (current_group_.GetName() != Group::kInvalidName) {
      groups_.emplace_back(current_group_);
    }
    return true;
  }

  // empty line
  if (tokens->Accept(kNewLine)) {
    return GroupEntry(tokens);
  }

  // comment
  if (tokens->Current().symbol == kPoundSign) {
    Comment(tokens);
    return GroupEntry(tokens);
  }

  // start of new Group
  if (tokens->Current().symbol == kLeftBracket) {
    return GroupParser(tokens);
  }

  // start of a new entry
  // key = value
  std::string key{tokens->Current().match};
  if (!tokens->Accept(kIdentifier)) {
    return false;
  }

  // localized entry?
  // key[locale] = value
  std::string locale;
  if (tokens->Accept(kLeftBracket)) {
    std::vector<parser::Token> skipped;
    if (!tokens->SkipUntil(kRightBracket, &skipped)) {
      return false;
    }
    tokens->Accept(kRightBracket);
    locale = parser::TokenList::JoinSkipped(skipped);
  }

  tokens->SkipOver(kWhitespace);

  if (!tokens->Accept(kEqualsSign)) {
    return false;
  }

  tokens->SkipOver(kWhitespace);

  std::vector<parser::Token> skipped;
  if (!tokens->SkipUntil(kNewLine, &skipped)) {
    return false;
  }

  if (!AddKeyValue(key, parser::TokenList::JoinSkipped(skipped), locale)) {
    return false;
  }

  // skip over the actual newline
  tokens->Next();

  return GroupEntry(tokens);
}

bool Parser::AddKeyValue(std::string key, std::string value,
                         std::string locale) {
  if (key == "NoDisplay" || key == "Hidden" || key == "Terminal" ||
      key == "StartupNotify" || key == "DBusActivatable") {
    // These keys are associated to boolean values.
    bool value_boolean;
    if (!ParseBooleanValue(value, &value_boolean)) {
      return false;
    }
    current_group_.AddEntry(key, value_boolean);
  } else if (key == "OnlyShowIn" || key == "NotShowIn" || key == "Actions" ||
             key == "MimeType" || key == "Categories" || key == "Implements") {
    Group::StringList value_string_list;
    if (!ParseStringListValue(value, &value_string_list)) {
      return false;
    }
    current_group_.AddEntry(key, value_string_list);
  } else if (key == "Version") {
    // Special-case "Version" which expects a string. The below code would
    // otherwise first try reading the value as a number -- which is of a
    // different type, though the version often looks like a number, so...
    if (!ParseStringValue(&value)) {
      return false;
    }
    current_group_.AddEntry(key, value);
  } else {
    // Try a numeric value.
    float value_numeric;
    if (ParseNumericValue(value, &value_numeric)) {
      current_group_.AddEntry(key, value_numeric);
    }
    // If all else fails, default to a string value.
    else if (ParseStringValue(&value)) {
      if (locale.empty()) {
        current_group_.AddEntry(key, value);
      } else {
        if (current_group_.IsEntry<std::string>(key)) {
          // Locale string which extends a previously non-localized entry
          std::string nonlocalized_value{
              current_group_.GetEntry<std::string>(key)};
          current_group_.AddEntry(key,
                                  Group::LocaleString{
                                      {"", nonlocalized_value}, {locale, value},
                                  });
        } else if (!current_group_.IsEntry<Group::LocaleString>(key)) {
          // Locale string, new
          current_group_.AddEntry(key, Group::LocaleString{{locale, value}});
        } else {
          // Locale string, existing, new locale
          auto& locale_map = current_group_.GetEntry<Group::LocaleString>(key);
          locale_map[locale] = value;
        }
      }
    }
    // Bail out.
    else {
      return false;
    }
  }

  return true;
}

bool ParseBooleanValue(std::string value_string, bool* value_boolean) {
  util::string::Trim(value_string);

  // Also accept "0" and "1" for backwards compatibility:
  //  https://specifications.freedesktop.org/desktop-entry-spec/latest/apc.html

  if (value_string == "true" || value_string == "1") {
    (*value_boolean) = true;
    return true;
  }

  if (value_string == "false" || value_string == "0") {
    (*value_boolean) = false;
    return true;
  }

  return false;
}

bool ParseNumericValue(std::string value_string, float* value_numeric) {
  std::istringstream ss{util::string::Trim(value_string)};

  if (!(ss >> *value_numeric)) {
    // Couldn't parse the floating point number.
    return false;
  }

  // Successful execution only if we parsed the number and consumed all the
  // data in the stream.
  return ss.eof();
}

bool ParseStringValue(std::string* value_string) {
  std::string value_parsed{*value_string};
  bool is_escape = false;

  auto it = value_parsed.begin();
  while (it != value_parsed.end()) {
    if (std::iscntrl(*it)) {
      util::log::Error() << "String value cannot contain control characters.\n";
      return false;
    }

    if (*it == '\\') {
      if (is_escape) {
        is_escape = false;
        ++it;
      } else {
        is_escape = true;
        it = value_parsed.erase(it);
      }
    } else {
      if (is_escape) {
        switch (*it) {
          case 's':
            (*it) = ' ';
            break;
          case 'n':
            (*it) = '\n';
            break;
          case 't':
            (*it) = '\t';
            break;
          case 'r':
            (*it) = '\r';
            break;
          case ';':
            (*it) = ';';
            break;
          default:
            util::log::Error() << "Invalid escape sequence: \\" << (*it)
                               << '\n';
            return false;
        }
      }
      is_escape = false;
      ++it;
    }
  }

  // Trailing slashes are not valid as they leave an unterminated escape
  // sequence behind.
  if (is_escape) {
    util::log::Error() << "Unterminated escape sequence.\n";
    return false;
  }

  value_string->assign(value_parsed);
  return true;
}

bool ParseStringListValue(std::string value_string,
                          Group::StringList* value_string_list) {
  value_string_list->clear();

  bool is_escape = false;
  auto begin = value_string.begin();

  for (auto it = value_string.begin(); it != value_string.end(); ++it) {
    if (*it == '\\') {
      is_escape = (!is_escape);
      continue;
    }
    if (*it == ';') {
      if (is_escape) {
        is_escape = false;
      } else {
        std::string slice{begin, it};
        if (!ParseStringValue(&slice)) {
          return false;
        }
        value_string_list->emplace_back(slice);
        begin = (it + 1);
      }
    }
  }
  if (begin != value_string.end()) {
    std::string slice{begin, value_string.end()};
    if (!ParseStringValue(&slice)) {
      return false;
    }
    value_string_list->emplace_back(slice);
  }
  return true;
}

bool Validate(DesktopEntry* entry) {
  if (entry->empty()) {
    util::log::Error() << "Desktop entry is empty.\n";
    return false;
  }

  auto& first = entry->at(0);
  if (first.GetName() != "Desktop Entry") {
    util::log::Error() << "The first group has to be \"Desktop Entry\".\n";
    return false;
  }

  if (!first.HasEntry("Name")) {
    util::log::Error() << "Desktop entry missing required field \"Name\".\n";
    return false;
  }

  if (!first.HasEntry("Type")) {
    util::log::Error() << "Desktop entry missing required field \"Type\".\n";
    return false;
  }

  if (first.GetEntry<std::string>("Type") == "Link" && !first.HasEntry("URL")) {
    util::log::Error() << "Desktop entry of type \"Link\" missing required "
                       << "field \"URL\".\n";
    return false;
  }

  Group::StringList actions;
  if (first.IsEntry<Group::StringList>("Action")) {
    actions = first.GetEntry<Group::StringList>("Action");
  }

  // Skip the first group "Desktop Entry", already validated above.
  auto it = (entry->begin() + 1);

  while (it != entry->end()) {
    std::string group_name{it->GetName()};
    if (!util::string::StartsWith(group_name, "Desktop Action ")) {
      util::log::Error() << "Invalid group name \"" << group_name << "\".\n";
      return false;
    }

    if (std::find(actions.begin(), actions.end(), group_name) ==
        actions.end()) {
      util::log::Debug() << "Group name \"" << group_name << "\" does not "
                         << "match any value in the \"Action\" entry. "
                         << "Ignoring.\n";
      it = entry->erase(it);
    } else {
      ++it;
    }
  }

  return true;
}

std::string BestLocalizedEntry(Group& group, std::string const& key) {
  if (group.IsEntry<std::string>(key)) {
    return group.GetEntry<std::string>(key);
  }

  auto& localized_name = group.GetEntry<Group::LocaleString>(key);
  auto current_locale = setlocale(LC_MESSAGES, nullptr);
  auto it = localized_name.find(current_locale);
  return (it != localized_name.end()) ? it->second : localized_name.at("");
}

}  // namespace desktop_entry
}  // namespace launcher
