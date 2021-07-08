// Copyright (C) 2020  Matthew "strager" Glazar
// See end of file for extended copyright information.

#ifndef QUICK_LINT_JS_RC_STRING_H
#define QUICK_LINT_JS_RC_STRING_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <quick-lint-js/assert.h>
#include <quick-lint-js/math-overflow.h>

namespace quick_lint_js {
// A sometimes-reference-counted null-terminated string.
//
// An rc_string can either manage its own string (which is reference-counted) or
// refer to a foreign string pointer (which is not reference-counted).
//
// rc_string is not thread-safe.
class rc_string {
 public:
  // Create a pointer to an existing string.
  //
  // This function does not copy the data. The returned rc_string becomes
  // invalid (but destructable) when the given c_string is deallocated or
  // mutated.
  static rc_string adopt_c_string(const char* c_string) noexcept {
    return rc_string(adopt_tag(), c_string);
  }

  // Create a reference-counted string copied from the given C string.
  static rc_string copy_c_string(const char* c_string) noexcept(false) {
    return rc_string(copy_tag(), c_string);
  }

  explicit rc_string() noexcept : data_(this->encoded_empty_string()) {}

  rc_string(const rc_string& other) noexcept : data_(other.data_) {
    this->increment_ref_count();
  }

  rc_string(rc_string&& other) noexcept
      : data_(std::exchange(other.data_, this->encoded_empty_string())) {
    other.increment_ref_count();
  }

  rc_string& operator=(const rc_string& other) noexcept {
    if (this != &other) {
      this->decrement_ref_count();
      this->data_ = other.data_;
      this->increment_ref_count();
    }
    return *this;
  }

  rc_string& operator=(rc_string&& other) noexcept {
    if (this != &other) {
      this->decrement_ref_count();
      this->data_ = std::exchange(other.data_, this->encoded_empty_string());
      other.increment_ref_count();
    }
    return *this;
  }

  ~rc_string() noexcept { this->decrement_ref_count(); }

  const char* c_str() const noexcept { return this->decode(this->data_); }

 private:
  using ref_count_type = int;

  static constexpr ref_count_type initial_ref_count = 0;

  // If no_ref_count is set in a data pointer, the pointer was adopted and
  // refers to a C-string not allocated by rc_string and is *not* a pointer to
  // ref_counted_string_data::data.
  //
  // If no_ref_count is unset, the pointer was allocated by rc_string and is a
  // pointer to ref_counted_string_data::data.
#if defined(__x86_64__) || defined(_M_X64)
  static constexpr std::uintptr_t no_ref_count_mask = 1ULL << 63;
  static constexpr std::uintptr_t no_ref_count_bits = 1ULL << 63;
#else
#error "Unsupported platform"
#endif

  struct ref_counted_string_data {
    ref_count_type ref_count;
    char data[1];
  };

  struct adopt_tag {};
  struct copy_tag {};

  explicit rc_string(adopt_tag, const char* c_string) noexcept {
    this->data_ = this->encode_not_ref_counted(c_string);
  }

  explicit rc_string(copy_tag, const char* c_string) noexcept(false) {
    std::size_t length = std::strlen(c_string);
    std::size_t size = length + 1;
    ref_counted_string_data* data = reinterpret_cast<ref_counted_string_data*>(
        std::malloc(size + offsetof(ref_counted_string_data, data)));
    data = new (data) ref_counted_string_data;
    data->ref_count = initial_ref_count;
    std::memcpy(data->data, c_string, size);
    this->data_ = this->encode_not_ref_counted(data->data);
  }

  void increment_ref_count() noexcept {
    if (this->is_ref_counted()) {
      std::optional<ref_count_type> new_ref_count =
          checked_add(this->ref_count(), 1);
      QLJS_ASSERT(new_ref_count.has_value());
      this->ref_count() = *new_ref_count;
    }
  }

  void decrement_ref_count() noexcept {
    if (this->is_ref_counted()) {
      ref_count_type ref_count = this->ref_count();
      if (ref_count == initial_ref_count) {
        std::free(this->allocated_string_data());
      } else {
        this->ref_count() = ref_count - 1;
      }
    }
  }

  ref_count_type& ref_count() noexcept {
    QLJS_ASSERT(this->is_ref_counted());
    return this->allocated_string_data()->ref_count;
  }

  ref_counted_string_data* allocated_string_data() noexcept {
    QLJS_ASSERT(this->is_ref_counted());
    return reinterpret_cast<ref_counted_string_data*>(
        this->data_ - offsetof(ref_counted_string_data, data));
  }

  bool is_ref_counted() const noexcept {
    return this->is_ref_counted(this->data_);
  }

  static bool is_ref_counted(std::uintptr_t p) noexcept {
    return (p & no_ref_count_mask) != no_ref_count_bits;
  }

  // Mark a pointer as ref-counted (i.e. it is part of a ref_counted_string_data
  // object).
  static std::uintptr_t encode_ref_counted(const char* p) noexcept {
    std::uintptr_t encoded = reinterpret_cast<std::uintptr_t>(p);
    QLJS_ASSERT(is_ref_counted(encoded));
    return encoded;
  }

  // Mark a pointer as not ref-counted (i.e. it is not part of a
  // ref_counted_string_data object).
  static std::uintptr_t encode_not_ref_counted(const char* p) noexcept {
    std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(p);
    // If the following assertion fails, it's likely that
    // no_ref_count_mask/no_ref_count_bits is incorrect for your platform.
    QLJS_ASSERT(is_ref_counted(raw));

    std::uintptr_t encoded = (raw & ~no_ref_count_mask) | no_ref_count_bits;
    QLJS_ASSERT(!is_ref_counted(encoded));
    return encoded;
  }

  // Get the pointer to the string data, restoring the ref-counted bits.
  static char* decode(std::uintptr_t p) noexcept {
    return reinterpret_cast<char*>((p | no_ref_count_mask) &
                                   ~no_ref_count_bits);
  }

  static std::uintptr_t encoded_empty_string() noexcept {
    return encode_not_ref_counted("");
  }

  std::uintptr_t data_;
};
}

#endif

// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew "strager" Glazar
//
// This file is part of quick-lint-js.
//
// quick-lint-js is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// quick-lint-js is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with quick-lint-js.  If not, see <https://www.gnu.org/licenses/>.
