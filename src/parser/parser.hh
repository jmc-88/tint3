#ifndef TINT3_PARSER_PARSER_HH
#define TINT3_PARSER_PARSER_HH

#include <string>
#include <vector>

#include "parser/lexer.hh"

namespace parser {

class TokenList {
public:
  explicit TokenList(Lexer::Result tokens);

  Token const& Current() const;
  bool Accept(Symbol symbol);
  bool Next();
  bool SkipOver(Symbol symbol, std::vector<Token>* output = nullptr);
  bool SkipUntil(Symbol symbol, std::vector<Token>* output = nullptr);

  static std::string JoinSkipped(std::vector<Token> const& tokens);

private:
  Lexer::Result tokens_;
  unsigned int current_;
};

class ParseCallback {
public:
  virtual bool operator()(TokenList* tokens) = 0;
};

class Parser {
public:
  Parser(Lexer lexer, ParseCallback* entry_fn);

  bool Parse(std::string const& buffer) const;

private:
  Lexer lexer_;
  ParseCallback* parser_entry_fn_;
};

}  // namespace parser

#endif  // TINT3_PARSER_PARSER_HH
