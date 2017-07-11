#ifndef TINT3_UTIL_PANGO_HH
#define TINT3_UTIL_PANGO_HH

#include <pango/pango.h>

namespace util {
namespace pango {

class FontDescriptionPtr {
 public:
  FontDescriptionPtr();
  FontDescriptionPtr(FontDescriptionPtr const& other);
  FontDescriptionPtr(FontDescriptionPtr&& other);
  ~FontDescriptionPtr();

  FontDescriptionPtr& operator=(FontDescriptionPtr other);
  FontDescriptionPtr& operator=(PangoFontDescription* ptr);
  PangoFontDescription* operator()() const;

  static FontDescriptionPtr FromPointer(PangoFontDescription* ptr);

 private:
  FontDescriptionPtr(PangoFontDescription* ptr);

  PangoFontDescription* font_description_;
};

}  // namespace pango
}  // namespace util

#endif  // TINT3_UTIL_PANGO_HH
