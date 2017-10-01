#include "catch.hpp"

#include <stack>
#include <string>

#include "parser/parser.hh"

enum TestFileTokens {
  kAdd = (parser::kEOF + 1),
  kMultiply,
  kLeftBracket,
  kRightBracket,
  kNumber,
  kWhitespace
};

static const parser::Lexer test_file_lexer{
    std::make_pair('+', kAdd),          std::make_pair('*', kMultiply),
    std::make_pair('(', kLeftBracket),  std::make_pair(')', kRightBracket),
    std::make_pair("[0-9-]+", kNumber), std::make_pair("\\s+", kWhitespace),
};

class SimpleArithmeticParser : public parser::ParseCallback {
  // <exp> ::= <exp> "+" <exp> | <exp> "*" <exp> | "(" <exp> ")" | [0-9]+

 public:
  bool operator()(parser::TokenList* tokens) { return Expression(tokens); }

  void Clear() {
    while (!op_stack_.empty()) {
      op_stack_.pop();
    }
    op_stack_.emplace(0);
  }

  unsigned int Result() const { return op_stack_.top(); }

 private:
  std::stack<unsigned int> op_stack_;

  bool Expression(parser::TokenList* tokens) {
    if (tokens->Current().symbol == kNumber) {
      op_stack_.emplace(std::stoul(tokens->Current().match));
      tokens->Next();
    } else if (tokens->Current().symbol == kLeftBracket) {
      if (!WrappedExpression(tokens)) {
        return false;
      }
    } else {
      return false;
    }

    tokens->SkipOver(kWhitespace);

    if (tokens->Accept(kAdd)) {
      tokens->SkipOver(kWhitespace);
      if (!Expression(tokens)) {
        return false;
      }

      unsigned int op2 = op_stack_.top();
      op_stack_.pop();
      unsigned int op1 = op_stack_.top();
      op_stack_.pop();
      op_stack_.emplace(op1 + op2);
    } else if (tokens->Accept(kMultiply)) {
      tokens->SkipOver(kWhitespace);
      if (!Expression(tokens)) {
        return false;
      }

      unsigned int op2 = op_stack_.top();
      op_stack_.pop();
      unsigned int op1 = op_stack_.top();
      op_stack_.pop();
      op_stack_.emplace(op1 * op2);
    }

    return true;
  }

  bool WrappedExpression(parser::TokenList* tokens) {
    if (!tokens->Accept(kLeftBracket)) {
      return false;
    }
    if (!Expression(tokens)) {
      return false;
    }
    return tokens->Accept(kRightBracket);
  }
};

TEST_CASE("Parser", "Parses valid contents and rejects invalid ones") {
  SimpleArithmeticParser simple_arithmetic_parser;
  parser::Parser test_parser{test_file_lexer, &simple_arithmetic_parser};

  // valid expressions
  simple_arithmetic_parser.Clear();
  REQUIRE(test_parser.Parse("7"));
  REQUIRE(simple_arithmetic_parser.Result() == 7);

  simple_arithmetic_parser.Clear();
  REQUIRE(test_parser.Parse("2 * 6"));
  REQUIRE(simple_arithmetic_parser.Result() == 12);

  simple_arithmetic_parser.Clear();
  REQUIRE(test_parser.Parse("(5 * 2) + (3 * (1 + 2))"));
  REQUIRE(simple_arithmetic_parser.Result() == 19);

  // invalid expressions
  simple_arithmetic_parser.Clear();
  REQUIRE_FALSE(test_parser.Parse(""));

  simple_arithmetic_parser.Clear();
  REQUIRE_FALSE(test_parser.Parse("()"));

  simple_arithmetic_parser.Clear();
  REQUIRE_FALSE(test_parser.Parse("1 + 2 * 3 ()"));

  simple_arithmetic_parser.Clear();
  REQUIRE_FALSE(test_parser.Parse("5 - 3"));
}
