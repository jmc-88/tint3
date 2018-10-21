#ifndef TINT3_UTIL_COLLECTION_HH
#define TINT3_UTIL_COLLECTION_HH

#include <algorithm>
#include <iterator>

template <typename T, typename V>
typename T::iterator erase(T& container, V const& value) {
  return container.erase(
    std::remove(std::begin(container), std::end(container), value),
    std::end(container));
}

template <typename T, typename P>
typename T::iterator erase_if(T& container, P unary_predicate) {
  return container.erase(
    std::remove_if(std::begin(container), std::end(container), unary_predicate),
    std::end(container));
}

#endif  // TINT3_UTIL_COLLECTION_HH
