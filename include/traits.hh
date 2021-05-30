#ifndef IVANP_TRAITS_HH
#define IVANP_TRAITS_HH

template <typename T, typename...> struct pack_head { using type = T; };
template <typename... T> using head_t = typename pack_head<T...>::type;

#endif
