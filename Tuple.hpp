#include "CppML/CppML.hpp"
#include <tuple>
using Predicate =
    ml::Apply<ml::UnList<ml::Get<1, ml::AligmentOf<>>>, ml::Greater<>>;

template <typename Id, typename T> struct Tag {};
template <typename... Ts>
using TaggedPermutation =
    ml::Invoke<ml::ZipWith<Tag, ml::Sort<Predicate>>,
               typename ml::TypeRange<>::template f<0, sizeof...(Ts)>,
               ml::ListT<Ts...>>;

template <int N, typename List, typename Pipe = ml::ToList>
using Extract =
    ml::Invoke<ml::UnList<ml::Apply<ml::UnList<ml::Get<N>>, Pipe>>, List>;

template <typename... Ts> class Tuple {
  using _TaggedPermutation = TaggedPermutation<Ts...>;
  using Permutation = Extract<0, _TaggedPermutation>;
  using _Tuple = Extract<1, _TaggedPermutation, ml::F<std::tuple>>;
  template <typename T>
  using Finder =
      ml::Invoke<ml::UnList<ml::FindIf<ml::Partial<ml::IsSame<>, T>>>,
                 Permutation>;
  using InversePermutation =
      typename ml::TypeRange<ml::Apply<ml::WrapIn1<Finder>>>::template f<
          0, ml::Length<Permutation>::value>;
  template <int I>
  using Index = ml::Invoke<ml::UnList<ml::Get<I>>, InversePermutation>;

  _Tuple _tuple;
  template <int... Is, typename... Us>
  Tuple(ml::ListT<ml::Int<Is>...>, std::tuple<Us &&...> &&fwd)
      : _tuple{static_cast<Us &&>(std::get<Is>(fwd))...} {}

public:
  template <typename... Us>
  Tuple(Us &&... us)
      : Tuple{Permutation{}, std::forward_as_tuple(static_cast<Us &&>(us)...)} {
  }
  template <int I, typename... Us> friend decltype(auto) get(Tuple<Us...> &tup);
  template <int I, typename... Us>
  friend decltype(auto) get(const Tuple<Us...> &tup);
};

template <int I, typename... Ts> decltype(auto) get(const Tuple<Ts...> &tup) {
  using _I = typename Tuple<Ts...>::template Index<I>;
  return std::get<_I::value>(tup._tuple);
}
template <int I, typename... Ts> decltype(auto) get(Tuple<Ts...> &tup) {
  using _I = typename Tuple<Ts...>::template Index<I>;
  return std::get<_I::value>(tup._tuple);
}

template <int... Is, template <class...> class T, typename... Ts,
          template <class...> class U, typename... Us, typename F>
decltype(auto) envoker(ml::ListT<ml::Int<Is>...>, const T<Ts...> &t,
                       const U<Us...> &u, F &&f) {
  using std::get;
  return (... && f(get<Is>(t), get<Is>(u)));
}

template <typename... Ts, typename... Us>
auto operator==(const std::tuple<Ts...> &lhs, const Tuple<Us...> &rhs) -> bool {
  using List = ml::TypeRange<>::f<0, sizeof...(Ts)>;
  return envoker(List{}, lhs, rhs, [](auto &&x, auto &&y) { return x == y; });
};

template <typename... Ts, typename... Us>
auto operator==(const Tuple<Us...> &lhs, const std::tuple<Ts...> &rhs) -> bool {
  return rhs == lhs;
};
