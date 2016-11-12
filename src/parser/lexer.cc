#include "parser/lexer.hh"

#include <regex>
#include <utility>

namespace parser {

TokenMatcher::TokenMatcher(TokenMatcher const& other)
    : matcher_(other.matcher_) {}

TokenMatcher::TokenMatcher(TokenMatcher&& other)
    : matcher_(std::move(other.matcher_)) {}

TokenMatcher::TokenMatcher(MatcherCallback matcher) : matcher_(matcher) {}

TokenMatcher::TokenMatcher(char c) : matcher_(MakeCharacterMatcher(c)) {}

TokenMatcher::TokenMatcher(const char* regexp)
    : matcher_(MakeRegexpMatcher(regexp)) {}

TokenMatcher::TokenMatcher(std::string const& regexp)
    : matcher_(MakeRegexpMatcher(regexp)) {}

TokenMatcher& TokenMatcher::operator=(TokenMatcher other) {
  std::swap(matcher_, other.matcher_);
  return *this;
}

bool TokenMatcher::operator()(std::string const& buffer, unsigned int* position,
                              std::string* output) const {
  return matcher_(buffer, position, output);
}

TokenMatcher::MatcherFunction TokenMatcher::MakeCharacterMatcher(char c) {
  return [c](std::string const& buffer, unsigned int* position,
             std::string* output) -> bool {
    if (buffer[*position] == c) {
      output->assign(1, c);
      (*position) += 1;
      return true;
    }
    return false;
  };
}

TokenMatcher::MatcherFunction TokenMatcher::MakeRegexpMatcher(
    std::string const& pattern) {
  std::regex regexp{pattern};
  return [regexp](std::string const& buffer, unsigned int* position,
                  std::string* output) -> bool {
    std::string remainder{buffer, (*position)};
    std::smatch match;
    if (std::regex_search(remainder, match, regexp,
                          std::regex_constants::match_continuous)) {
      output->assign(match.str());
      (*position) += match.length();
      return true;
    }
    return false;
  };
}

Token::Token(Symbol symbol, unsigned int begin, unsigned int end,
             std::string const& match)
    : symbol(symbol), begin(begin), end(end), match(match) {}

Lexer::Lexer(Lexer const& other)
    : matcher_to_symbol_(other.matcher_to_symbol_) {}

Lexer::Lexer(Lexer&& other)
    : matcher_to_symbol_(std::move(other.matcher_to_symbol_)) {}

Lexer::Lexer(std::initializer_list<Lexer::MatchPair> const& match_map)
    : matcher_to_symbol_(match_map) {}

Lexer& Lexer::operator=(Lexer other) {
  std::swap(matcher_to_symbol_, other.matcher_to_symbol_);
  return *this;
}

bool Lexer::ProcessContents(std::string const& buffer, Result* result) const {
  unsigned int length = buffer.length();
  unsigned int i = 0;

  result->clear();

  while (i < length) {
    bool found_match = false;
    for (auto& pair : matcher_to_symbol_) {
      TokenMatcher matcher = pair.first;
      Symbol symbol = pair.second;

      std::string output;
      unsigned int begin = i;
      if (matcher(buffer, &i, &output)) {
        found_match = true;
        result->push_back(Token(symbol, begin, i, output));
        break;
      }
    }
    if (!found_match) {
      return false;
    }
  }

  result->push_back(Token(kEOF, length, length, ""));
  return true;
}

}  // namespace parser
