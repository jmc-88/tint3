#include <utility>

#include "launcher/desktop_entry.hh"

namespace launcher {
namespace desktop_entry {

const parser::Lexer kLexer{
    std::make_pair('\n', kNewLine),
    std::make_pair("\\s+", kWhitespace),
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

Parser::Parser() : current_group_(Group::kInvalidName) {}

bool Parser::operator()(parser::TokenList* tokens) {
  return DesktopEntry(tokens);
}

std::vector<Group> Parser::GetGroups() const { return groups_; }

bool Parser::DesktopEntry(parser::TokenList* tokens) {
  // end of file, stop parsing
  if (tokens->Accept(parser::kEOF)) {
    return true;
  }

  // empty line
  if (tokens->Accept(kNewLine)) {
    return DesktopEntry(tokens);
  }

  // comment line
  if (tokens->Current().symbol == kPoundSign) {
    Comment(tokens);
    return DesktopEntry(tokens);
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

  // key = value
  std::string key{tokens->Current().match};
  if (!tokens->Accept(kIdentifier)) {
    return false;
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

  std::string value{parser::TokenList::JoinSkipped(skipped)};

  // TODO: use the correct type here according to the specification:
  //  https://specifications.freedesktop.org/desktop-entry-spec/latest/ar01s05.html
  current_group_.AddEntry(key, value);

  // skip over the actual newline
  tokens->Next();

  return GroupEntry(tokens);
}

}  // namespace desktop_entry
}  // namespace launcher
