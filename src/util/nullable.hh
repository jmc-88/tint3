#ifndef TINT3_UTIL_NULLABLE_HH
#define TINT3_UTIL_NULLABLE_HH

#include <algorithm>
#include <ostream>
#include <utility>

namespace util {

template <typename T>
class Nullable {
 public:
  Nullable() : has_value_(false) {}
  explicit Nullable(T const& value) : has_value_(true), value_(value) {}
  explicit Nullable(T&& value) : has_value_(true), value_(std::move(value)) {}

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

template <typename T>
util::Nullable<T> Some(T const& value) {
  return util::Nullable<T>(value);
}

template <typename T>
util::Nullable<T> Some(T&& value) {
  return util::Nullable<T>(value);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, util::Nullable<T> const& nullable) {
  if (nullable) {
    return os << "Some(" << nullable.Unwrap() << ")";
  } else {
    return os << "None";
  }
}

#endif  // TINT3_UTIL_NULLABLE_HH
