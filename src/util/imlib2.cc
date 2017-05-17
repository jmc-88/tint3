#include <algorithm>
#include <utility>

#include "util/common.hh"
#include "util/imlib2.hh"

namespace util {
namespace imlib2 {

namespace {

class ScopedCurrentImageRestorer {
 public:
  ScopedCurrentImageRestorer() : image_(imlib_context_get_image()) {}
  ~ScopedCurrentImageRestorer() { imlib_context_set_image(image_); }

 private:
  Imlib_Image image_;
};

Imlib_Image CloneImlib2Image(Imlib_Image other_image) {
  if (!other_image) {
    return nullptr;
  }
  ScopedCurrentImageRestorer restorer;
  imlib_context_set_image(other_image);
  return imlib_clone_image();
}

}  // namespace

Image::Image(Imlib_Image image) : image_(image) {}

Image::Image(Image const& other) : image_(CloneImlib2Image(other.image_)) {}

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

void Image::AdjustASB(int alpha, float saturation_adjustment,
                      float brightness_adjustment) {
  if (image_ != nullptr) {
    ScopedCurrentImageRestorer restorer;
    imlib_context_set_image(image_);
    DATA32* data = imlib_image_get_data();
    ::AdjustASB(data, imlib_image_get_width(), imlib_image_get_height(), alpha,
                saturation_adjustment, brightness_adjustment);
    imlib_image_put_back_data(data);
  }
}

void Image::Free() {
  if (image_ != nullptr) {
    ScopedCurrentImageRestorer restorer;
    imlib_context_set_image(image_);
    imlib_free_image();
    image_ = nullptr;
  }
}

Image Image::CloneExisting(Imlib_Image other_image) {
  return Image{CloneImlib2Image(other_image)};
}

}  // namespace imlib2
}  // namespace util
