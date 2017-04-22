#ifndef TINT3_UTIL_BIMAP_HH
#define TINT3_UTIL_BIMAP_HH

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace util {

// Implements a bidirectional map.
//
// Naming of this class and its methods is STL-like:
//  https://google.github.io/styleguide/cppguide.html#Exceptions_to_Naming_Rules

template <typename A, typename B, typename CompareA = std::less<A>,
          typename CompareB = std::less<B> >
class bimap {
 private:
  using bimap_node = std::pair<A, B>;

  class bimap_node_left_accessor {
   public:
    A const& operator()(bimap_node const* n) { return n->first; }
  };

  class bimap_node_right_accessor {
   public:
    B const& operator()(bimap_node const* n) { return n->second; }
  };

  using bimap_left_map = std::map<A, bimap_node*, CompareA>;
  using bimap_right_map = std::map<B, bimap_node*, CompareB>;

  template <typename T1, typename T2, typename CompareT1>
  class const_iterator {
   public:
    using return_pair = std::pair<const T1, const T2>;
    using t1_map = std::map<T1, bimap_node*, CompareT1>;
    using t1_map_iterator = typename t1_map::const_iterator;
    using node_other_accessor = std::function<T2 const&(bimap_node const*)>;

    const_iterator() = delete;

    const_iterator(t1_map const& map, t1_map_iterator const& it,
                   node_other_accessor other_accessor)
        : map_(map),
          it_(it),
          pair_(nullptr),
          node_other_accessor_(other_accessor) {
      if (it_ != map_.end()) {
        pair_.reset(
            new return_pair{it_->first, node_other_accessor_(it_->second)});
      }
    }

    const_iterator(const_iterator const& other)
        : map_(other.map_),
          it_(other.it_),
          pair_(other.pair_),
          node_other_accessor_(other.node_other_accessor_) {}

    const_iterator(const_iterator&& other)
        : map_(std::move(other.map_)),
          it_(std::move(other.it_)),
          pair_(std::move(other.pair_)),
          node_other_accessor_(std::move(other.node_other_accessor_)) {}

    const_iterator& operator=(const_iterator other) {
      std::swap(map_, other.map_);
      std::swap(it_, other.it_);
      std::swap(pair_, other.pair_);
      std::swap(node_other_accessor_, other.node_other_accessor_);
    }

    return_pair const& operator*() const { return *(pair_.get()); }

    return_pair const* operator->() const { return pair_.get(); }

    bool operator==(const_iterator const& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const_iterator const& other) const {
      return it_ != other.it_;
    }

    const_iterator& operator++() {
      if (++it_ != map_.end()) {
        pair_.reset(
            new return_pair{it_->first, node_other_accessor_(it_->second)});
      }
      return *this;
    }

    const_iterator operator++(int /* dummy_this_is_postincrement */) {
      const_iterator current{*this};
      ++(*this);
      return current;
    }

   private:
    t1_map const& map_;
    t1_map_iterator it_;
    std::unique_ptr<return_pair> pair_;
    node_other_accessor node_other_accessor_;
  };

  template <typename T1, typename T2, typename CompareT1, typename CompareT2>
  class side_handler {
   public:
    using node_other_accessor = std::function<T2 const&(bimap_node const*)>;
    using this_side_map = std::map<T1, bimap_node*, CompareT1>;
    using other_side_map = std::map<T2, bimap_node*, CompareT2>;
    using this_const_iterator = const_iterator<T1, T2, CompareT1>;

    explicit side_handler(this_side_map& this_side, other_side_map& other_side,
                          node_other_accessor other_accessor)
        : this_side_(this_side),
          other_side_(other_side),
          node_other_accessor_(other_accessor) {}

    bool has(T1 const& t) const {
      return this_side_.find(t) != this_side_.end();
    }

    bool erase(T1 const& t) {
      auto this_it = this_side_.find(t);
      if (this_it == this_side_.end()) {
        return false;
      }
      bimap_node const* node = this_it->second;
      auto other_it = other_side_.find(node_other_accessor_(node));
      if (other_it == other_side_.end()) {
        return false;
      }
      this_side_.erase(this_it);
      other_side_.erase(other_it);
      delete node;
      return true;
    }

    this_const_iterator begin() const { return cbegin(); }

    this_const_iterator cbegin() const {
      return this_const_iterator{this_side_, this_side_.cbegin(),
                                 node_other_accessor_};
    }

    this_const_iterator end() const { return cend(); }

    this_const_iterator cend() const {
      return this_const_iterator{this_side_, this_side_.cend(),
                                 node_other_accessor_};
    }

   private:
    this_side_map& this_side_;
    other_side_map& other_side_;
    node_other_accessor node_other_accessor_;
  };

  bimap_left_map left_;
  bimap_right_map right_;

 public:
  bimap()
      : left(left_, right_, bimap_node_right_accessor()),
        right(right_, left_, bimap_node_left_accessor()) {}

  ~bimap() {
    // Delete all the bimap_node* stored in the left_ map.
    // This is guaranteed to match all the nodes pointed to the right_ map, so
    // we won't have any memory leak.
    for (auto& p : left_) {
      delete p.second;
    }
  }

  side_handler<A, B, CompareA, CompareB> left;
  side_handler<B, A, CompareB, CompareA> right;

  bool empty() const {
    // Emptiness of one map is sufficient to test the emptiness of both.
    return left_.empty();
  }

  size_t size() const {
    // Size of one map is guaranteed to match the other.
    return left_.size();
  }

  bool insert(A const& a, B const& b) {
    if (left.has(a) || right.has(b)) {
      // Fail if already present.
      return false;
    }
    auto node = new (std::nothrow) bimap_node{a, b};
    if (!node) {
      // Fail if allocation fails.
      return false;
    }
    left_.insert(std::make_pair(a, node));
    right_.insert(std::make_pair(b, node));
    return true;
  }
};

}  // namespace util

#endif  // TINT3_UTIL_BIMAP_HH
