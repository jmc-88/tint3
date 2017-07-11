#include <util/pango.hh>

#include <algorithm>
#include <utility>

namespace util {
namespace pango {

FontDescriptionPtr::FontDescriptionPtr()
    : font_description_(pango_font_description_from_string("sans 10")) {}

FontDescriptionPtr::FontDescriptionPtr(PangoFontDescription* ptr)
    : font_description_(ptr) {}

FontDescriptionPtr::FontDescriptionPtr(FontDescriptionPtr const& other)
    : font_description_(pango_font_description_copy(other.font_description_)) {}

FontDescriptionPtr::FontDescriptionPtr(FontDescriptionPtr&& other)
    : font_description_(std::move(other.font_description_)) {
  other.font_description_ = nullptr;
}

FontDescriptionPtr::~FontDescriptionPtr() {
  if (font_description_) {
    pango_font_description_free(font_description_);
  }
}

FontDescriptionPtr& FontDescriptionPtr::operator=(FontDescriptionPtr other) {
  std::swap(font_description_, other.font_description_);
  return (*this);
}

FontDescriptionPtr& FontDescriptionPtr::operator=(PangoFontDescription* ptr) {
  if (font_description_ && font_description_ != ptr) {
    pango_font_description_free(font_description_);
  }
  font_description_ = ptr;
  return (*this);
}

PangoFontDescription* FontDescriptionPtr::operator()() const {
  return font_description_;
}

FontDescriptionPtr FontDescriptionPtr::FromPointer(PangoFontDescription* ptr) {
  return ptr;
}

}  // namespace pango
}  // namespace util
