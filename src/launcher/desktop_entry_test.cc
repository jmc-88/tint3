#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <clocale>

#include "parser/parser.hh"

#include "launcher/desktop_entry.hh"

// Example Desktop Entry File, as seen in the specification:
//  https://specifications.freedesktop.org/desktop-entry-spec/latest/apa.html
static constexpr char kExampleContents[] =
    u8R"EOF(
[Desktop Entry]
Version=1.0
Type=Application
Name=Foo Viewer
Comment=The best viewer for Foo objects available!
TryExec=fooview
Exec=fooview %F
Icon=fooview
MimeType=image/x-foo;
Actions=Gallery;Create;

[Desktop Action Gallery]
Exec=fooview --gallery
Name=Browse Gallery

[Desktop Action Create]
Exec=fooview --create-new
Name=Create a new Foo!
Icon=fooview-new
)EOF";

TEST_CASE("ExampleContents", "Correctly parses a valid .desktop entry file") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  // This should parse correctly.
  REQUIRE(p.Parse(kExampleContents));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};
  REQUIRE(groups.size() == 3);
  REQUIRE(groups[0].GetName() == "Desktop Entry");
  REQUIRE(groups[1].GetName() == "Desktop Action Gallery");
  REQUIRE(groups[2].GetName() == "Desktop Action Create");

  // Desktop Entry
  REQUIRE(groups[0].GetEntry<std::string>("Version") == "1.0");
  REQUIRE(groups[0].GetEntry<std::string>("Name") == "Foo Viewer");
  REQUIRE(groups[0].GetEntry<std::string>("Comment") ==
          "The best viewer for Foo objects available!");
  REQUIRE(groups[0].GetEntry<std::string>("TryExec") == "fooview");
  REQUIRE(groups[0].GetEntry<std::string>("Exec") == "fooview %F");
  REQUIRE(groups[0].GetEntry<std::string>("Icon") == "fooview");

  using StringList = launcher::desktop_entry::Group::StringList;
  auto mime_type = groups[0].GetEntry<StringList>("MimeType");
  REQUIRE(mime_type == (StringList{"image/x-foo"}));
  auto actions = groups[0].GetEntry<StringList>("Actions");
  REQUIRE(actions == (StringList{"Gallery", "Create"}));

  // Desktop Action Gallery
  REQUIRE(groups[1].GetEntry<std::string>("Exec") == "fooview --gallery");
  REQUIRE(groups[1].GetEntry<std::string>("Name") == "Browse Gallery");

  // Desktop Action Create
  REQUIRE(groups[2].GetEntry<std::string>("Exec") == "fooview --create-new");
  REQUIRE(groups[2].GetEntry<std::string>("Name") == "Create a new Foo!");
  REQUIRE(groups[2].GetEntry<std::string>("Icon") == "fooview-new");

  // It should also validate correctly.
  REQUIRE(launcher::desktop_entry::Validate(&groups));
}

static constexpr char kInvalidGroupOrder[] =
    u8R"EOF(
[Desktop Action In The Wrong Position]
Exec=/bin/true
Name=Whoops.

[Desktop Entry]
Type=Application
Name=Foo Viewer
)EOF";

TEST_CASE("InvalidGroupOrder", "Refuses entries with the wrong group order") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kInvalidGroupOrder));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};
  REQUIRE(!launcher::desktop_entry::Validate(&groups));
}

static constexpr char kInvalidActionGroups[] =
    u8R"EOF(
[Desktop Entry]
Type=Application
Name=Foo Viewer

[Desktop Action Something]
Exec=/bin/true
Name=Something

[Desktop Action Something Else]
Exec=/bin/sleep 3600
Name=Zzz
)EOF";

TEST_CASE("InvalidActionGroups", "Ignores invalid action groups") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kInvalidActionGroups));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};

  // Before: all three groups are parsed and returned.
  REQUIRE(groups.size() == 3);
  REQUIRE(groups[0].GetName() == "Desktop Entry");
  REQUIRE(groups[1].GetName() == "Desktop Action Something");
  REQUIRE(groups[2].GetName() == "Desktop Action Something Else");

  // Run the validation.
  REQUIRE(launcher::desktop_entry::Validate(&groups));

  // After: only the main "Desktop Entry" group should be retained, the others
  // should be removed as they don't match any item in the "Action" entry.
  REQUIRE(groups.size() == 1);
  REQUIRE(groups[0].GetName() == "Desktop Entry");
}

static constexpr char kValueTypes[] =
    u8R"EOF(
[Desktop Entry]
Type=Application
Name=Foo Viewer
StartupNotify=false
)EOF";

TEST_CASE("ValueTypes", "Correctly parses all supported value types") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kValueTypes));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};
  REQUIRE(launcher::desktop_entry::Validate(&groups));

  // Boolean value.
  REQUIRE(groups[0].HasEntry("StartupNotify"));
  REQUIRE(groups[0].IsEntry<bool>("StartupNotify"));
  REQUIRE(groups[0].GetEntry<bool>("StartupNotify") == false);
}

static constexpr char kLocalizedContents[] =
    u8R"EOF(
[Desktop Entry]
Version=1.0
Type=Application
Name=Foo Viewer
Name[it]=Visualizzatore di Foo
Name[fr]=Visionneuse de Foo
)EOF";

TEST_CASE("LocalizedContents", "Localized strings are handled correctly") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kLocalizedContents));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};
  REQUIRE(launcher::desktop_entry::Validate(&groups));

  // Locale string.
  using LocaleString = launcher::desktop_entry::Group::LocaleString;
  REQUIRE(groups[0].HasEntry("Name"));
  REQUIRE(groups[0].IsEntry<LocaleString>("Name"));

  LocaleString& str = groups[0].GetEntry<LocaleString>("Name");
  REQUIRE(str[""] == "Foo Viewer");
  REQUIRE(str["it"] == "Visualizzatore di Foo");
  REQUIRE(str["fr"] == "Visionneuse de Foo");
}

TEST_CASE("ParseBooleanValue", "Only accepts 'true' and 'false'") {
  bool value_boolean;

  REQUIRE(launcher::desktop_entry::ParseBooleanValue("true", &value_boolean));
  REQUIRE(value_boolean);

  REQUIRE(launcher::desktop_entry::ParseBooleanValue("1", &value_boolean));
  REQUIRE(value_boolean);

  REQUIRE(launcher::desktop_entry::ParseBooleanValue("false", &value_boolean));
  REQUIRE(!value_boolean);

  REQUIRE(launcher::desktop_entry::ParseBooleanValue("0", &value_boolean));
  REQUIRE(!value_boolean);

  REQUIRE(launcher::desktop_entry::ParseBooleanValue(" true ", &value_boolean));
  REQUIRE(value_boolean);

  REQUIRE(!launcher::desktop_entry::ParseBooleanValue("", &value_boolean));
  REQUIRE(!launcher::desktop_entry::ParseBooleanValue("yes", &value_boolean));
}

TEST_CASE("ParseNumericValue", "Correctly parses strings containing floats") {
  float value_numeric;

  REQUIRE(launcher::desktop_entry::ParseNumericValue("10.123", &value_numeric));
  REQUIRE(value_numeric == 10.123f);

  REQUIRE(launcher::desktop_entry::ParseNumericValue("0.0001", &value_numeric));
  REQUIRE(value_numeric == 0.0001f);

  REQUIRE(launcher::desktop_entry::ParseNumericValue(" 1.1 ", &value_numeric));
  REQUIRE(value_numeric == 1.1f);

  REQUIRE(!launcher::desktop_entry::ParseNumericValue("", &value_numeric));
  REQUIRE(!launcher::desktop_entry::ParseNumericValue("pea", &value_numeric));
  REQUIRE(!launcher::desktop_entry::ParseNumericValue("1.1 2.2 3.3",
                                                      &value_numeric));
}

TEST_CASE("ParseStringValue", "Correctly reads escape sequences") {
  std::string value_string;

  value_string = "plain";
  REQUIRE(launcher::desktop_entry::ParseStringValue(&value_string));

  value_string = u8R"S(hey\;\\some\sescape\tsequences\nhere\r)S";
  REQUIRE(launcher::desktop_entry::ParseStringValue(&value_string));
  REQUIRE(value_string == "hey;\\some escape\tsequences\nhere\r");

  value_string = u8R"S(invalid escape sequence: \%)S";
  REQUIRE(!launcher::desktop_entry::ParseStringValue(&value_string));

  value_string = "control characters: \n";
  REQUIRE(!launcher::desktop_entry::ParseStringValue(&value_string));
}

TEST_CASE("ParseStringListValue", "Correctly reads string lists") {
  using StringList = launcher::desktop_entry::Group::StringList;
  StringList value_string_list;
  std::string value_string;

  value_string = "plain";
  REQUIRE(launcher::desktop_entry::ParseStringListValue(value_string,
                                                        &value_string_list));
  REQUIRE(value_string_list == (StringList{"plain"}));

  value_string = ";leading semicolon";
  REQUIRE(launcher::desktop_entry::ParseStringListValue(value_string,
                                                        &value_string_list));
  REQUIRE(value_string_list == (StringList{"", "leading semicolon"}));

  value_string = "trailing semicolon;";
  REQUIRE(launcher::desktop_entry::ParseStringListValue(value_string,
                                                        &value_string_list));
  REQUIRE(value_string_list == (StringList{"trailing semicolon"}));

  value_string = "multiple;elements;";
  REQUIRE(launcher::desktop_entry::ParseStringListValue(value_string,
                                                        &value_string_list));
  REQUIRE(value_string_list == (StringList{"multiple", "elements"}));

  value_string = u8R"S(semicolons\; they are handled correctly;)S";
  REQUIRE(launcher::desktop_entry::ParseStringListValue(value_string,
                                                        &value_string_list));
  REQUIRE(value_string_list ==
          (StringList{"semicolons; they are handled correctly"}));
}

TEST_CASE("BestLocalizedEntry", "Picks the best value for the current locale") {
  using LocaleString = launcher::desktop_entry::Group::LocaleString;

  SECTION("Non-localized entry") {
    launcher::desktop_entry::Group entry{"Mock Entry"};
    entry.AddEntry<std::string>("Key", "Non-localized");
    REQUIRE(launcher::desktop_entry::BestLocalizedEntry(entry, "Key") ==
            "Non-localized");
  }

  SECTION("Localized entry, C locale") {
    launcher::desktop_entry::Group entry{"Mock Entry"};
    entry.AddEntry<LocaleString>("Key",
                                 LocaleString{
                                     {"", "Non-localized"}, {"C", "C locale"},
                                 });

    // "C" appears in the above map, return the proper string.
    setlocale(LC_MESSAGES, "C");
    REQUIRE(launcher::desktop_entry::BestLocalizedEntry(entry, "Key") ==
            "C locale");
  }

  SECTION("Localized entry, unknown locale") {
    launcher::desktop_entry::Group entry{"Mock Entry"};
    entry.AddEntry<LocaleString>(
        "Key", LocaleString{
                   {"", "Non-localized"}, {"xx_XX", "Unknown locale"},
               });

    // "C" doesn't appear in the above map, fall back to non-localized string.
    setlocale(LC_MESSAGES, "C");
    REQUIRE(launcher::desktop_entry::BestLocalizedEntry(entry, "Key") ==
            "Non-localized");
  }
}

// Trailing whitespace on the "Comment" line.
// Not a heredoc string as editors would likely remove the trailing whitespace
// on save.
static constexpr char kTrailingWhitespace[] =
    "[Desktop Entry]\n"
    "Name=Firefox\n"
    "Type=Application\n"
    "Exec=firefox\n"
    "Comment=Browse the World Wide Web \n"
    "GenericName=Web Browser\n";

TEST_CASE("TrailingWhitespace",
          "Trailing whitespace is consumed without swallowing a newline") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kTrailingWhitespace));

  launcher::desktop_entry::DesktopEntry groups{
      desktop_entry_parser.GetDesktopEntry()};
  REQUIRE(launcher::desktop_entry::Validate(&groups));

  // Check both entries are present, and the newline separating them wasn't
  // swallowed by an overly eager lexer.
  REQUIRE(groups[0].GetEntry<std::string>("GenericName") == "Web Browser");
  REQUIRE(groups[0].GetEntry<std::string>("Comment") ==
          "Browse the World Wide Web ");
}
