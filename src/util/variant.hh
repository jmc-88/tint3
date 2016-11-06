#ifndef TINT3_UTIL_VARIANT_HH
#define TINT3_UTIL_VARIANT_HH

#include <functional>
#include <type_traits>

// Implementation of a variant type.
//
// Heavily inspired by:
//  https://www.ojdip.net/2013/10/implementing-a-variant-type-in-cpp/
//
// All this would be unnecessary if we had std::variant<>, but that's a C++17
// feature that's unimplemented in most compilers at this point.

namespace util {
namespace variant {

namespace {

template<size_t arg1, size_t ... others>
struct static_max;

template<size_t arg>
struct static_max<arg> {
  static const size_t value = arg;
};

template<size_t arg1, size_t arg2, size_t ... others>
struct static_max<arg1, arg2, others...> {
  static const size_t value = (arg1 >= arg2)
      ? static_max<arg1, others...>::value
      : static_max<arg2, others...>::value;
};

template<typename... Ts>
struct variant_helper;

template<> struct variant_helper<>  {
  inline static void destroy(size_t type_id, void* ptr) {}
  inline static void move(size_t type_id, void* old_ptr, void* new_ptr) {}
  inline static void copy(size_t type_id, const void* old_ptr, void* new_ptr) {}
};

template<typename T, typename... Ts>
struct variant_helper<T, Ts...> {
  inline static void destroy(size_t type_id, void* ptr) {
    if (type_id == typeid(T).hash_code()) {
      reinterpret_cast<T*>(ptr)->~T();
    } else {
      variant_helper<Ts...>::destroy(type_id, ptr);
    }
  }

  inline static void move(size_t type_id, void* old_ptr, void* new_ptr) {
    if (type_id == typeid(T).hash_code()) {
      new (new_ptr) T(std::move(*reinterpret_cast<T*>(old_ptr)));
    } else {
      variant_helper<Ts...>::move(type_id, old_ptr, new_ptr);
    }
  }

  inline static void copy(size_t type_id, const void* old_ptr, void* new_ptr) {
    if (type_id == typeid(T).hash_code()) {
      new (new_ptr) T(*reinterpret_cast<const T*>(old_ptr));
    } else {
      variant_helper<Ts...>::copy(type_id, old_ptr, new_ptr);
    }
  }
};

}  // namespace

template<typename... Ts>
class Value {
public:
  Value() : alive_(false), type_id_(typeid(void).hash_code()) {
  }

  template<typename T>
  Value(T value) : alive_(false), type_id_(typeid(void).hash_code()) {
    Set<T>(value);
  }

  Value(const Value<Ts...>& other) : type_id_(other.type_id_) {
    VariantHelper::copy(other.type_id_, &other.storage_, &storage_);
  }

  Value(Value<Ts...>&& other) : type_id_(other.type_id_) {
    VariantHelper::move(other.type_id_, &other.storage_, &storage_);
  }

  ~Value() {
    VariantHelper::destroy(type_id_, &storage_);
  }

  Value<Ts...>& operator=(Value other) {
    std::swap(alive_, other.alive_);
    std::swap(type_id_, other.type_id_);
    std::swap(storage_, other.storage_);
    return *this;
  }

  template<typename T>
  bool Is() {
    return (type_id_ == typeid(T).hash_code());
  }

  template<typename T>
  T& Get() {
    if (!Is<T>()) {
      throw std::bad_cast();
    }
    return *reinterpret_cast<T*>(&storage_);
  }

  template<typename T, typename... Args>
  void Set(Args&&... args) {
    VariantHelper::destroy(type_id_, &storage_);
    new (&storage_) T(std::forward<Args>(args)...);
    type_id_ = typeid(T).hash_code();
  }

private:
  using VariantHelper = variant_helper<Ts...>;
  using DataType = typename std::aligned_storage<
    static_max<sizeof(Ts)...>::value,
    static_max<alignof(Ts)...>::value
  >::type;

  bool alive_;
  size_t type_id_;
  DataType storage_;
};

}  // namespace variant
}  // namespace util

#endif  // TINT3_UTIL_VARIANT_HH
