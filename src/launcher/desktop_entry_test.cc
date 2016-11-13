#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "launcher/desktop_entry.hh"
#include "parser/parser.hh"

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

TEST_CASE("Parser", "Correctly parses a valid .desktop entry file") {
  launcher::desktop_entry::Parser desktop_entry_parser;
  parser::Parser p{launcher::desktop_entry::kLexer, &desktop_entry_parser};

  REQUIRE(p.Parse(kExampleContents));

  std::vector<launcher::desktop_entry::Group> groups{
      desktop_entry_parser.GetGroups()};
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
  REQUIRE(groups[0].GetEntry<std::string>("MimeType") ==
          "image/x-foo;");  // TODO: stringlist
  REQUIRE(groups[0].GetEntry<std::string>("Actions") ==
          "Gallery;Create;");  // TODO: stringlist

  // Desktop Action Gallery
  REQUIRE(groups[1].GetEntry<std::string>("Exec") == "fooview --gallery");
  REQUIRE(groups[1].GetEntry<std::string>("Name") == "Browse Gallery");

  // Desktop Action Create
  REQUIRE(groups[2].GetEntry<std::string>("Exec") == "fooview --create-new");
  REQUIRE(groups[2].GetEntry<std::string>("Name") == "Create a new Foo!");
  REQUIRE(groups[2].GetEntry<std::string>("Icon") == "fooview-new");
}
