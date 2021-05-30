#ifndef IVANP_STRING_HH
#define IVANP_STRING_HH

#include <string>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace ivanp {

[[nodiscard]]
inline std::string cat() noexcept { return { }; }
[[nodiscard]]
inline std::string cat(std::string x) noexcept { return x; }
[[nodiscard]]
inline std::string cat(const char* x) noexcept { return x; }
[[nodiscard]]
inline std::string cat(char x) noexcept { return std::string(1,x); }
[[nodiscard]]
inline std::string cat(std::string_view x) noexcept { return std::string(x); }

template <typename... T>
[[nodiscard]]
[[gnu::always_inline]]
inline auto cat(T... x) -> std::enable_if_t<
  (sizeof...(T) > 1) && (std::is_same_v<T,std::string_view> && ...),
  std::string
> {
  std::string s;
  s.reserve((x.size() + ...));
  (s += ... += x);
  return s;
}

namespace impl {

[[nodiscard]]
inline std::string_view to_string_view(std::string_view x) noexcept {
  return x;
}
[[nodiscard]]
inline std::string_view to_string_view(const char& x) noexcept {
  return { &x, 1 };
}

}

template <typename... T>
[[nodiscard]]
[[gnu::always_inline]]
inline auto cat(const T&... x) -> std::enable_if_t<
  (sizeof...(T) > 1) && !(std::is_same_v<T,std::string_view> && ...),
  std::string
> {
  return cat(impl::to_string_view(x)...);
}

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T>
  bool operator()(const T& a, const char* b) const noexcept {
    return a < b;
  }
  template <typename T>
  bool operator()(const char* a, const T& b) const noexcept {
    return a < b;
  }
};

} // end namespace ivanp

#endif
