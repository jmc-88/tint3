#ifndef TINT3_UTIL_NULLABLE_HH
#define TINT3_UTIL_NULLABLE_HH

#include <algorithm>
#include <utility>

namespace util {

template <typename T>
class Nullable {
 public:
  Nullable() : has_value_(false) {}
  Nullable(T const& value) : has_value_(true), value_(value) {}
  Nullable(T&& value) : has_value_(true), value_(std::move(value)) {}

  operator bool() const { return has_value_; }

  operator T const&() const { return Unwrap(); }

  Nullable& operator=(Nullable other) {
    std::swap(has_value_, other.has_value_);
    std::swap(value_, other.value_);
    return *this;
  }

  T const& Unwrap() const { return value_; }

  void Clear() {
    has_value_ = false;
    value_ = T();
  }

 private:
  bool has_value_;
  T value_;
};

}  // namespace util

#endif  // TINT3_UTIL_NULLABLE_HH
