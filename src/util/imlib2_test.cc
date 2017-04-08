#include "catch.hpp"

#include <random>
#include <utility>

#include "util/imlib2.hh"

TEST_CASE("imlib2::Image::Image", "Construction/destruction") {
  util::imlib2::Image null_image;
  REQUIRE(null_image == nullptr);

  // AddressSanitizer should make sure imlib_empty_image gets freed by the
  // destructor and we're not leaking memory.
  Imlib_Image imlib_empty_image = imlib_create_image(100, 100);
  util::imlib2::Image empty_image{imlib_empty_image};
  REQUIRE(empty_image != nullptr);
  REQUIRE(empty_image == imlib_empty_image);

  util::imlib2::Image copy_of_empty_image{empty_image};
  REQUIRE(copy_of_empty_image != nullptr);
  REQUIRE(copy_of_empty_image != empty_image);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif  // __clang__
  // Lambda that returns a temporary util::imlib2::Image object, to test the
  // move constructor also works.
  auto new_imlib2_image = []() -> util::imlib2::Image {
    return imlib_create_image(100, 100);
  };
  util::imlib2::Image moved_image{std::move(new_imlib2_image())};
  REQUIRE(moved_image != nullptr);
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__
}

TEST_CASE("imlib2::Image::operator=", "Assignment") {
  // Assignment of an Imlib_Image object.
  Imlib_Image imlib_image1 = imlib_create_image(100, 100);
  util::imlib2::Image image1;
  REQUIRE(image1 == nullptr);
  image1 = imlib_image1;
  REQUIRE(image1 == imlib_image1);

  // Assignment of an util::imlib2::Image object.
  // We'll verify:
  //  1) the object wraps a non-null pointer;
  //  2) the object is no longer assigned to imlib_image1;
  //  3) the object has cloned the Imlib_Image from image2, so it doesn't wrap
  //     the same pointer value.
  util::imlib2::Image image2{imlib_create_image(100, 100)};
  image1 = image2;
  REQUIRE(image1 != nullptr);
  REQUIRE(image1 != imlib_image1);
  REQUIRE(image1 != image2);
}

TEST_CASE("imlib2::Image::AdjustASB", "Adjustment in place works") {
  // Prepare a 1x1 image to adjust.
  util::imlib2::Image image{imlib_create_image(1, 1)};
  imlib_context_set_image(image);
  imlib_image_set_has_alpha(1);
  DATA32* original_data = imlib_image_get_data();
  original_data[0] = 0xffc86464;  // rgba(200, 100, 100, 1.0)
  imlib_image_put_back_data(original_data);

  image.AdjustASB(50, 0.0, +0.1);
  DATA32* adjusted_data = imlib_image_get_data();
  REQUIRE(adjusted_data[0] == 0x7fe27171);  // rgba(226, 113, 113, 0.5)
  REQUIRE(adjusted_data == original_data);  // data was not reallocated
}

TEST_CASE("imlib2::Image::CloneExisting",
          "Returns an Image object holding a clone of the given Imlib_Image") {
  static constexpr unsigned int const width = 10;
  static constexpr unsigned int const height = 10;

  Imlib_Image original = imlib_create_image(width, height);
  util::imlib2::Image original_cleanup{original};  // to free the image

  imlib_context_set_image(original);
  DATA32* original_data = imlib_image_get_data();

  std::random_device device;
  std::mt19937 generator{device()};
  std::normal_distribution<> distribution{
      std::numeric_limits<unsigned int>::min(),
      std::numeric_limits<unsigned int>::max()};

  for (unsigned int i = 0; i < width * height; ++i) {
    original_data[i] = distribution(generator);
  }

  auto clone = util::imlib2::Image::CloneExisting(original);
  REQUIRE(clone != nullptr);
  REQUIRE(clone != original);

  imlib_context_set_image(clone);
  DATA32* clone_data = imlib_image_get_data();

  for (unsigned int i = 0; i < width * height; ++i) {
    REQUIRE(clone_data[i] == original_data[i]);
  }
}
