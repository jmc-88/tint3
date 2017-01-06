#ifndef TINT3_UTIL_IMLIB2
#define TINT3_UTIL_IMLIB2

#include <Imlib2.h>

namespace util {
namespace imlib2 {

class Image {
 public:
  Image(Imlib_Image image = nullptr);
  Image(Image const& other);
  Image(Image&& other);
  ~Image();

  Image& operator=(Image other);
  Image& operator=(Imlib_Image image);
  operator Imlib_Image() const;

  void Free();

  static Image CloneExisting(Imlib_Image other_image);

 private:
  Imlib_Image image_;
};

}  // namespace imlib2
}  // namespace util

#endif  // TINT3_UTIL_IMLIB2
