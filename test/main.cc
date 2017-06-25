#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <locale.h>
#include <cstdlib>

#include <cairo/cairo.h>
#include <pango/pangocairo.h>

// https://github.com/philsquared/Catch/blob/master/docs/own-main.md
int main(int argc, char* argv[]) {
  // Make GLib code more Valgrind-friendly.
  //  https://developer.gnome.org/glib/unstable/glib-running.html
  setlocale(LC_ALL, "");
  setenv("G_DEBUG", "gc-friendly", 1);
  setenv("G_SLICE", "always-malloc", 1);

  int result = Catch::Session().run(argc, argv);

  // Pango and Cairo use Fontconfig, which handles memory in "unconventional"
  // ways to deal with mmap()-ed cache files, which confuses both Valgrind and
  // LeakSanitizer. These additional cleanup steps reduce the amount of false
  // positives somewhat.
  pango_cairo_font_map_set_default(nullptr);
  cairo_debug_reset_static_data();

  return (result < 0xff ? result : 0xff);
}
