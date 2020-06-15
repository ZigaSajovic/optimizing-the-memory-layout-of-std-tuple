#include "CppML/CppML.hpp"
#include <tuple>

template <typename Permutation, typename Tuple> struct TupleBase;
template <int... Is, typename... Ts>
struct TupleBase<ml::ListT<ml::Int<Is>...>, std::tuple<Ts...>> {
private:
  std::tuple<Ts...> _tuple;
  template <typename... Us>
  TupleBase(ml::_, std::tuple<Us...> &&fwd)
      : _tuple{
            static_cast<ml::f<ml::Get<Is>, Us...> &&>(std::get<Is>(fwd))...} {}

public:
  template <typename... Us>
  TupleBase(Us &&... us)
      : TupleBase{ml::_{}, std::forward_as_tuple(static_cast<Us &&>(us)...)} {}
  template <typename I> // Compute the inverse index
  using f = ml::f<ml::FindIdIf<ml::Partial<ml::IsSame<>, I>>, ml::Int<Is>...>;
  template <int I, typename... Us>
  friend decltype(auto) get(TupleBase<Us...> &tup);
  template <int I, typename... Us>
  friend decltype(auto) get(const TupleBase<Us...> &tup);
};

template <int I, typename... Us> decltype(auto) get(TupleBase<Us...> &tup) {
  return std::get<ml::f<TupleBase<Us...>, ml::Int<I>>::value>(tup._tuple);
}

template <int I, typename... Us>
decltype(auto) get(const TupleBase<Us...> &tup) {
  return std::get<ml::f<TupleBase<Us...>, ml::Int<I>>::value>(tup._tuple);
}

template <typename... Ts>
using MakeBase = ml::f<
    ml::ZipWith<
        ml::ListT,
        ml::Sort<ml::Map<ml::Unwrap<ml::Get<1, ml::AlignOf<>>>, ml::Greater<>>,
                 ml::Product<ml::Map<ml::Unwrap<ml::Get<0>>>,
                             ml::Map<ml::Unwrap<ml::Get<1>>, ml::F<std::tuple>>,
                             ml::F<TupleBase>>>>,
    ml::Range<>::f<0, sizeof...(Ts)>, ml::ListT<Ts...>>;

template <typename... Ts> struct Tuple : MakeBase<Ts...> {
  using MakeBase<Ts...>::MakeBase;
};

template <int... Is, template <class...> class T, typename... Ts,
          template <class...> class U, typename... Us, typename F>
decltype(auto) envoker(ml::ListT<ml::Int<Is>...>, const T<Ts...> &t,
                       const U<Us...> &u, F &&f) {
  using std::get;
  return (... && f(get<Is>(t), get<Is>(u)));
}

template <typename... Ts, typename... Us>
auto operator==(const std::tuple<Ts...> &lhs, const Tuple<Us...> &rhs) -> bool {
  using List = ml::Range<>::f<0, sizeof...(Ts)>;
  return envoker(List{}, lhs, rhs, [](auto &&x, auto &&y) { return x == y; });
};

template <typename... Ts, typename... Us>
auto operator==(const Tuple<Us...> &lhs, const std::tuple<Ts...> &rhs) -> bool {
  return rhs == lhs;
};
