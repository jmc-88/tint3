#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "parser/lexer.hh"

TEST_CASE("Lexer", "Tokens are returned as expected") {
  enum TestFileTokens {
    kNewLine = (parser::kEOF + 1),
    kPoundSign,
    kLeftBracket,
    kRightBracket,
    kEqualsSign,
    kIdentifier,
    kWhitespace,
    kAny,
  };

  static constexpr char kTestContents[] =
      u8R"EOF(
# random comment
[section header]
key=value
)EOF";

  static constexpr parser::Symbol kExpectedSequence[] = {
      // Line 1, empty
      kNewLine,
      // Line 2
      kPoundSign, kWhitespace, kIdentifier, kWhitespace, kIdentifier, kNewLine,
      // Line 3
      kLeftBracket, kIdentifier, kWhitespace, kIdentifier, kRightBracket,
      kNewLine,
      // Line 4
      kIdentifier, kEqualsSign, kIdentifier, kNewLine,
      // End of file
      parser::kEOF,
  };

  static constexpr unsigned int kExpectedSequenceLength =
      (sizeof(kExpectedSequence) / sizeof(parser::Symbol));

  static const parser::Lexer lexer{
      std::make_pair('\n', kNewLine),
      std::make_pair("\\s+", kWhitespace),
      std::make_pair('#', kPoundSign),
      std::make_pair('[', kLeftBracket),
      std::make_pair(']', kRightBracket),
      std::make_pair('=', kEqualsSign),
      std::make_pair("[A-Za-z0-9-]+", kIdentifier),
      std::make_pair(".", kAny),
  };

  parser::Lexer::Result result;
  REQUIRE(lexer.ProcessContents(kTestContents, &result));
  REQUIRE(result.size() == kExpectedSequenceLength);
  for (unsigned int i = 0; i < kExpectedSequenceLength; ++i) {
    REQUIRE(result[i].symbol == kExpectedSequence[i]);
  }
}