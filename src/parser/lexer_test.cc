#include "catch.hpp"

#include "parser/lexer.hh"

TEST_CASE("matchers::NewLine", "NewLine matcher is sane") {
  SECTION("<CR><LF>") {
    std::string cr_lf{"\r\n"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::NewLine(cr_lf, &position, &output));
    REQUIRE(position == cr_lf.length());
    REQUIRE(output == cr_lf);
  }

  SECTION("<CR>") {
    std::string cr{"\r"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::NewLine(cr, &position, &output));
    REQUIRE(position == cr.length());
    REQUIRE(output == cr);
  }

  SECTION("<LF>") {
    std::string lf{"\n"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::NewLine(lf, &position, &output));
    REQUIRE(position == lf.length());
    REQUIRE(output == lf);
  }

  SECTION("<LF><CR><LF>") {
    std::string mixed{"\n\r\n"};
    unsigned int position = 0;
    std::string output;
    // First pass: consume only '\n'
    REQUIRE(parser::matcher::NewLine(mixed, &position, &output));
    REQUIRE(position == 1);
    REQUIRE(output == "\n");
    // Second pass: consume '\r\n'
    REQUIRE(parser::matcher::NewLine(mixed, &position, &output));
    REQUIRE(position == mixed.length());
    REQUIRE(output == "\r\n");
  }
}

TEST_CASE("matchers::Whitespace", "Whitespace matcher is sane") {
  SECTION("All spaces") {
    std::string all_spaces{"   "};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::Whitespace(all_spaces, &position, &output));
    REQUIRE(position == all_spaces.length());
    REQUIRE(output == all_spaces);
  }

  SECTION("Mixed spaces") {
    std::string mixed_spaces{" \t \r"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::Whitespace(mixed_spaces, &position, &output));
    REQUIRE(position == mixed_spaces.length());
    REQUIRE(output == mixed_spaces);
  }

  SECTION("Leading spaces") {
    std::string test_string{"   test"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::Whitespace(test_string, &position, &output));
    REQUIRE(position == 3);
    REQUIRE(output == "   ");
  }

  SECTION("Trailing spaces") {
    std::string test_string{"test   "};
    unsigned int position = 0;
    std::string output;
    // Since we're starting from position=0, there's no whitespace that matches
    // at that position, so we expect to fail here.
    REQUIRE(!parser::matcher::Whitespace(test_string, &position, &output));
    REQUIRE(position == 0);
    REQUIRE(output.empty());
  }
}

TEST_CASE("matchers::Any", "Whitespace matcher is sane") {
  SECTION("Inside a string") {
    std::string test_string{"test"};
    unsigned int position = 0;
    std::string output;
    REQUIRE(parser::matcher::Any(test_string, &position, &output));
    REQUIRE(position == 1);
    REQUIRE(output == "t");
  }

  SECTION("At the end of a string") {
    std::string test_string{"test"};
    unsigned int position = test_string.length();
    std::string output;
    REQUIRE(!parser::matcher::Any(test_string, &position, &output));
    REQUIRE(position == test_string.length());
    REQUIRE(output.empty());
  }
}

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
