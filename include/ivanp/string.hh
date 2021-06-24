#ifndef IVANP_STRING_HH
#define IVANP_STRING_HH

#include <string>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace ivanp {

[[nodiscard]]
inline const char* cstr(const char* x) noexcept { return x; }
[[nodiscard]]
inline const char* cstr(const std::string& x) noexcept { return x.c_str(); }

namespace impl {

[[nodiscard]]
[[gnu::always_inline]]
inline std::string_view to_string_view(std::string_view x) noexcept {
  return x;
}
[[nodiscard]]
[[gnu::always_inline]]
inline std::string_view to_string_view(const char& x) noexcept {
  return { &x, 1 };
}

[[gnu::always_inline]]
inline void join1(std::string& s, std::string_view d, std::string_view x) {
  if (x.size()) {
    if (s.size() && d.size()) s += d;
    s += x;
  }
}

template <typename T>
inline constexpr bool is_sv = std::is_same_v<T,std::string_view>;

}

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
  (sizeof...(x) > 1) && (impl::is_sv<T> && ...),
  std::string
> {
  std::string s;
  s.reserve((x.size() + ...));
  (s += ... += x);
  return s;
}

template <typename... T>
[[nodiscard]]
[[gnu::always_inline]]
inline auto cat(const T&... x) -> std::enable_if_t<
  (sizeof...(x) > 1) && !(impl::is_sv<T> && ...),
  std::string
> {
  return cat(impl::to_string_view(x)...);
}

template <typename... T>
[[nodiscard]]
inline auto join(std::string_view d, T... x) -> std::enable_if_t<
  (impl::is_sv<T> && ...),
  std::string
> {
  std::string s;
  if constexpr (sizeof...(x)==1) {
    ( (s += x), ... );
  } else if constexpr (sizeof...(x)>1) {
    const size_t dlen = d.size();
    size_t len = ( (x.size() ? x.size()+dlen : 0) + ... );
    if (len) len -= dlen;
    s.reserve(len);
    ( impl::join1(s,d,x), ... );
  }
  return s;
}

template <typename D, typename... T>
[[nodiscard]]
inline auto join(const D& d, const T&... x)
-> std::enable_if_t<
  !(impl::is_sv<D> && ... && impl::is_sv<T>),
  std::string
> {
  return join(impl::to_string_view(d),impl::to_string_view(x)...);
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
