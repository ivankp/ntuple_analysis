#ifndef IVANP_YCOMBINATOR_HH
#define IVANP_YCOMBINATOR_HH

namespace ivanp {

template <typename F>
struct Ycombinator {
  F f;
  template <typename... Args>
  decltype(auto) operator()(Args&&... args) const {
    return f(*this, std::forward<Args>(args)...);
  }
};

}

#endif
