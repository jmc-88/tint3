#include <algorithm>
#include <utility>

#include "util/imlib2.hh"

namespace util {
namespace imlib2 {

Image::Image(Imlib_Image image) : image_(image) {}

Image::Image(Image const& other) : image_(nullptr) {
  Imlib_Image current_image = imlib_context_get_image();
  imlib_context_set_image(other.image_);
  image_ = imlib_clone_image();
  imlib_context_set_image(current_image);
}

Image::Image(Image&& other) : image_(std::move(other.image_)) {}

Image::~Image() { Free(); }

Image& Image::operator=(Image other) {
  std::swap(image_, other.image_);
  return (*this);
}

Image& Image::operator=(Imlib_Image image) {
  Free();
  image_ = image;
  return (*this);
}

Image::operator Imlib_Image() const { return image_; }

void Image::Free() {
  if (image_ != nullptr) {
    Imlib_Image current_image = imlib_context_get_image();
    imlib_context_set_image(image_);
    imlib_free_image();
    image_ = nullptr;
    imlib_context_set_image(current_image);
  }
}

}  // namespace imlib2
}  // namespace util
