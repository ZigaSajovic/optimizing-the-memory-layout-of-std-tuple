# Optimizing the memory layout of std::tuple

In the last few years I have become increasingly interested in bringing higher order concepts of category theory closer to the bits that implement their instances. This leads one to languages like *C++*, where the types have insight into the hardware, which gives the constructs control over how they are mapped onto it. On the way towards of such meta-endeavours I created **CppML**, a [metalanguage for C++](https://github.com/ZigaSajovic/CppML), which I use when developing libraries.
In this text, we will use it to optimize the memory layout of `std::tuple`, at no runtime or cognitive cost on the end of the user.

Before we begin, have a look at the result.

```c++

  Tuple<char, int, char, int, char, double, char> tup{'a', 1,   'c', 3,
                                                      'd', 5.0, 'e'};
  std::cout << "Size of Tuple: " << sizeof(tup) << " Bytes" << std::endl;

  std::tuple<char, int, char, int, char, double, char> std_tup{'a', 1,   'c', 3,
                                                               'd', 5.0, 'e'};
  std::cout << "Size of std::tuple: " << sizeof(std_tup) << " Bytes"
            << std::endl;

  std::cout << "Actual size of data: "
            << 4 * sizeof(char) + 2 * sizeof(int) + sizeof(double) << " Bytes"
            << std::endl;

  std::cout << get<2>(tup) << " == " << std::get<2>(std_tup) << std::endl;
  assert(tup == std_tup);
```
---
> Size of Tuple:  24 Bytes  
Size of std::tuple: 40 Bytes  
Actual size of data: 20 Bytes  
c == c
---
We notice that the *std::tuple* has **20 Bytes** of **wasted** space (making it **twice** as big as the actual data), while *Tuple* only has **4 Bytes** of **wasted** space.

| **class**  | **size [B]** | **efficiency** |  
|:----------:|:------------:|:--------------:|  
| Data       | 20           | 1              |  
| Tuple      | 24           | 0.84           |  
| std::tuple | 40           | 0.5            |  

The solution spans roughly `70` lines of code, which we will build up step by step in this *README*. Please note that it does not contain all the functionalities required of `std::tuple`, but it does provide all the non-trivial implementations (hence others are trivially implementable in terms (or in light) of those provided). The entire code can be found [here](https://github.com/ZigaSajovic/optimizing-the-memory-layout-of-std-tuple/blob/master/Tuple.hpp).

Please note that this text is not intended to be a tutorial on [CppML](https://github.com/ZigaSajovic/CppML), for that please see the in-depth [`CppML Tutorial`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md). Regardless, we will still include explanations and illustrative examples of the steps we take here. Note that all the links with the `ml::` prefix (like [`ml::ZipWith`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/ZipWith.md)) lead to the metafunctions entry in the [`CppML Reference`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/index.md), which you are encouraged to follow.

## Where is the slack in std::tuple

You can imagine `std::tuple<T0, T1, ..., Tn>` as a class, with members `T0 t0`, `T1 t1`, ..., `Tn tn`, laid out in the specified (or reversed) order. As such, like in any class, padding needs to be inserted between the members, so that they are *naturally aligned*, which generally means that the data's memory address is a multiple of the data size (have a look at the [Data structure alignment](https://en.wikipedia.org/wiki/Data_structure_alignment)). This means that the order of the members will influence the size of the objects of that type. This is why tuples with same types, but in different order, can have different sizes.

```c++
std::tuple<char, int, char> t_big;
std::tuple<int, char, char> t_small;
std::cout << "Size of t_big: " << sizeof(t_big) <<std::endl;
std::cout << "Size of t_small: " << sizeof(t_small) <<std::endl;
```
---
> Size of t_big: 12  
Size of t_small: 8
---

## Formulating the problem

Given that the order of elements has an impact on the size of the objects of that class we can turn towards finding the permutation of the order of members that minimizes the needed padding. But checking each permutation has *superexponential* complexity (see [Sterlings formula](https://en.wikipedia.org/wiki/Stirling%27s_approximation)). But we can approximate this process, by sorting the elements by their alignment. You can see this, by imagining aligning elements, which have been sorted by size:

Lets say you are aligning the first element `T`, and it needs padding to be naturally aligned. All the elements with the same size as `T` will follow it in the sequence, and wont need padding. The first element `U` that comes with a different size from `T` will need some padding. Than all the elements of same size that follow it, will not. And so on.

## Formulating the solution

We implement a class `Tuple`, which is an interface wrapper around `std::tuple`. It works by approximating the optimal permutation by the `Permutation` that sorts the types by their `alignment`. It than lays the objects out in memory in that order. It holds the `Permutation` as its template argument, and uses it to internally redirect the users indexing (hence the user can be oblivious to the permutation).

We want a `TupleBase` wrapper of a `std::tuple`, which will

```c++
template <typename Permutation, typename StdTuple> struct TupleBase;
template<int ...Is, typename ...Ts>
struct TupleBase<
            ml::ListT<ml::Int<Is>...>,
            std::tuple<Ts...>> {
/* Implementation */
};
```

have the `Permuatation` that sorts the types by their `alignment` as its first template parameter, and the already permuted `std::tuple` as its second. Hence, we need a `MakeBase` metafunction, which will allow us to implement `Tuple` class like

```c++
template <typename... Ts> struct Tuple : MakeBase<Ts...> {
  using MakeBase<Ts...>::MakeBase;
};
```

On a concrete example, we want `MakeBase`

```c++
using TB0 = MakeBase<char, int, char, int, char, double, char>;
using TB1 = 
    TupleBase<ml::ListT<ml::Int<5>, ml::Int<3>, ml::Int<1>, ml::Int<6>, ml::Int<4>,
                        ml::Int<2>, ml::Int<0>>,
              std::tuple<double, int, int, char, char, char, char>>;
static_assert(
      std::is_same_v<TB0, TB1>);
```



We will first write the [metaprogram that computes the permutations](#makebase-metaprogram), and than code the [interface indirecting wrapper](#the-tuple-wrapper-class).

### `MakeBase` Metaprogram

To get the permutation and its inverse, we take the list of types, enumerate (zip with a tag) them (so we can track their position), and sort them by their alignment. We than extract the enumerates from the sequence as our permutation and extract the sorted types into a `std::tuple`.

Below we specify the **metaprogram** in a bullet-list, which we will than translate step by step into `CppML`.


* Zip with `Tag` (using [`ml::ZipWith`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/ZipWith.md))
  * the list of type-integers in the range `[0, sizeof...(Ts))`, (which is created using [`ml::Range`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Range.md))
    * [`ml::ListT`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Vocabulary/List.md)`<`[`ml::Int`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Vocabulary/Value.md)`<0>, ..., `[`ml::Int`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Vocabulary/Value.md)`<sizeof...(Ts) - 1>>`, and
  * the list made from `Ts...`
    * [`ml::ListT`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Vocabulary/List.md)`<Ts...>`
* [`ml::Sort`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/Sort.md) the resulting parameter pack `Tag<ml::Int<Is>, Ts>...`, with the `Comparator` that takes the `alignment`of the `T`.
  * `Comparator: P0, P1 -> Bool<t>`
  * We [`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md) (the `P0` and `P1`) by:
    * [`ml::Unwrap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Unwrap.md) the parameter pack from `Tag<Int<I>, T>` (see [`Unwrapping template arguments into metafunctions`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md#unwrapping-template-arguments-into-metafunctions)), and
    * extract the second element (`T`) using [`ml::Get`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Get.md), and
    * pipe the extracted `T` into [`ml::AlignOf`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/TypeTraits/AlignOf.md)
  * and pipe the alignments into [`ml::Greater`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Arithmetic/Greater.md)
* We than split the sorted parameter pack `Tag<ml::Int<Is>, Ts>...` into `TupleBase<ml::ListT<ml::Int<Is>...>, std::tuple<Ts...>>` by:
  * create a [`ml::ProductMap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/ProductMap.md) of:
    * [`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md) of extractors of the `ml::Int<i>`:
      * [`ml::Unwrap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Unwrap.md) the parameter pack from `Tag<Int<I>, T>` (see [`Unwrapping template arguments into metafunctions`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md#unwrapping-template-arguments-into-metafunctions)), and
      * extract the first element (`ml::Int<I>`) using [`ml::Get`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Get.md), and
      * pipe into [`ml::ToList`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/ToList.md)
    * [`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md) of extractors of the `T`:
      * [`ml::Unwrap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Unwrap.md) the parameter pack from `Tag<Int<I>, T>` (see [`Unwrapping template arguments into metafunctions`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md#unwrapping-template-arguments-into-metafunctions)), and
      * extract the second element (`T`) using [`ml::Get`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Get.md), and
      * pipe into the metafunction created from `std::tuple` (using [`ml::F`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/F.md); see [`Lifting templates to metafunctions`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md#lifting-templates-to-metafunctions)),
    * and `Pipe`into the metafunction created from `TupleBase` (using [`ml::F`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/F.md); see [`Lifting templates to metafunctions`](https://github.com/ZigaSajovic/CppML/blob/master/docs/tutorial/index.md#lifting-templates-to-metafunctions))

This sequence is easily translated into `CppML`:

```c++
template <typename ...Ts>
using MakeBase = ml::f<
    ml::ZipWith<
        Param,
        ml::Sort<ml::Map<ml::Unwrap<ml::Get<1, ml::AlignOf<>>>, ml::Greater<>>,
                 ml::Product<ml::Map<ml::Unwrap<ml::Get<0>>>,
                             ml::Map<ml::Unwrap<ml::Get<1>>, ml::F<std::tuple>>,
                             ml::F<TupleBase>>>>,
    ml::Range<>::f<0, sizeof...(Ts)>, ml::ListT<Ts...>>;
```

We will spend the rest of this post building the `MakeBase` metafunction step by step.

We will also need a metafunction that will compute the inverse permutation for an index `I`, which will allow us to internally redirect users indexing. This is done by locating the index of `I` in the permutation (using [`ml::FindIdIf`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/FindIdIf.md).

#### Enumerating the list of types

Our goal is two-fold. We wish to generate a permutation of elements, which will optimize the object size, while ensuring the **as if** rule. This means that the user is to be oblivious to the permutation behind the scene, and can access elements in the same order he/she originally specified. To this purpose, we will enumerate the types (i.e. **Zip** them with appropriate index) before permuting them.

[**CppML**](https://github.com/ZigaSajovic/CppML) provides the [`ml::ZipWith`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/ZipWith.md) metafunction, which takes `N` lists of types, and Zips them using the provided template. It also provides  [`ml::Range`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Range.md), which generates a list of integer constants `ml::ListT<ml::Int<0>, ..., ml::Int<N>>`.

```c++
template <typename Id, typename T> struct Tag {};
template <typename... Ts>
using TaggedList =
ml::f<ml::ZipWith<Tag // Type with which to Zip
                      // ml::ToList<> // is the implicit Pipe
                  >,
      typename ml::TypeRange<>::template f<
          0, sizeof...(Ts)>, // Integer sequence from [0, |Ts...|)
      ml::ListT<Ts...>>
```

For example

```c++
using Tagged = ml::ListT<Tag<ml::Int<0>, int>, Tag<ml::Int<1>, double>>;
static_assert(std::is_same_v<TaggedList<int, double>, Tagged>);

```

**Note** the implicit **Pipe** ([`ml::ToList`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/ToList.md)) in the above code section. **Pipe** is a key concept in [**CppML**](https://github.com/ZigaSajovic/CppML), it is how metafunction execution can be chained (think bash pipes). In the above code section, the implicit **Pipe** is ([`ml::ToList`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/ToList.md)), which returns the result in a list. We will be replacing it with [`ml::Sort`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/Sort.md) later (i.e. we will pipe the result of [`ml::ZipWith`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/ZipWith.md) into [`ml::Sort`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/Sort.md)).

#### Sorting the enumerated type list by the second element

As specified, we will use [`ml::Sort`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/Sort.md) as a greedy solution to the packing problem. [`ml::Sort`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/Sort.md)`<Comparator>` provided by [**CppML**](https://github.com/ZigaSajovic/CppML) operates on a parameter pack:

```c++
using SortedIs = ml::f<ml::Sort<ml::Greater<>>, // Predicate
                       ml::Int<0>, ml::Int<2>, ml::Int<1>>;
using Sorted = ml::ListT<ml::Int<2>, ml::Int<1>, ml::Int<0>>;
static_assert(std::is_same_v<SortedIs, Sorted>);
```

We need to write a predicate metafunction appropriate for out list of tagged types.

##### Constructing the predicate metafunction

The predicate is a metafunction mapping a pair of types to `ml::Bool<truth_val>`. The elements of the list are of the form `Tag<ml::Int<I0>, T0>`, and we wish to compare on
**Alignment**s of `T`s. Therefore our predicate is to be a metafunction that maps

```c++
(Tag<ml::Int<I0>, T0>, Tag<ml::Int<I1>, T1>)
->
(T0, T1)
>->
(ml::Int<alignment_of_T0>, ml::Int<alignment_of_T1>)
->
ml::Bool<truth_val>
```

We will achieve this by *taking a pack of two types* and  ([`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md))ing them by a metafunction that ([`ml::Unwrap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Unwrap.md))s the `Tag`, `pipes` the result to [`ml::Get`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Get.md)`<1>`, to extract the second element (being `Ti`), and pipe the result to [`ml::AlignOf`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/TypeTraits/AlignOf.md). We will than `pipe` the result of ([`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md)) to [`ml::Greater`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Arithmetic/Greater.md).

```c++
using Predicate = ml::Map<       // Map each of the two types:
    ml::Unwrap<                  // Unwrap the Tag
        ml::Get<1,               // Extract the second element
                ml::AlignOf<>>>, // Take its alignment
    ml::Greater<>>;              // And Pipe the result of Map to
                                 // Greater
```

For example

```c++
static_assert(
  std::is_same_v<
    ml::f<Predicate, Tag<ml::Int<2>, double>, Tag<ml::Int<5>, float>>,
    ml::Bool<true>>)
```

because alignment of `double` is greater than that of `float` (on my machine).

#### Computing the tagged permutation

We can now put it all together.

```c++
template <typename Id, typename T> struct Tag{};
template <typename... Ts>
using TaggedPermutation =
    ml::f<ml::ZipWith<Tag, // Zip the input lists with Tag and pipe into
                      ml::Sort<Predicate>>, // than compare the generated
                                            // elements (pipe-ing from the
                                            // Map) using Greater
          ml::Range<>::f<0, sizeof...(Ts)>, // generate a
                                                                  // range from
                                                                  // [0,
                                                                  // numOfTypes)
          ml::ListT<Ts...>>;
```

On a concrete example, this metafunction evaluates to

```c++
using TaggedPerm = TaggedPermutation<char, int, char, int, char, double, char>;
using List =
    ml::ListT<Tag<ml::Int<5>, double>, Tag<ml::Int<3>, int>,
              Tag<ml::Int<1>, int>, Tag<ml::Int<6>, char>,
              Tag<ml::Int<4>, char>, Tag<ml::Int<2>, char>,
              Tag<ml::Int<0>, char>>
static_assert(
              std::is_same_v<TaggedPerm, List>);
```

#### Extracting the permutation and the permuted tuple into a `TupleBase`

Examining the `TaggedPerm` above, it is essentially a list of Tags, `Tag<ml::Const<int, I>, T>`, with `I` being the original position of `T`. We now need to split this list into two lists, where the first will only hold the integer constants, and the other only the types. We accomplish this by ([`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md))ing each `Tag` with the [`ml::Get`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Pack/Get.md). Note that we will need to ([`ml::Unwrap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Unwrap.md)) the `Tag`, as ([`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md)) operates on parameter packs.

```c++
using Extract0 =
    ml::Map<                     // Mapping
        ml::Unwrap<ml::Get<0>>> // Get N-th element
```

and for the types

```c++
using Extract1 =
    ml::Map<                     // Mapping
        ml::Unwrap<ml::Get<1>,
        ml::F<std::tuple>>> // Get N-th element
```

This metafunctions can now be used on the computed `TaggedPerm`. **Note** that in the `Extract1` we are using a lifted template `std::tuple` as the `Pipe` of ([`ml::Map`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/Map.md)).

```c++
using Permutation =
    ml::ListT<ml::Int<5>, ml::Int<3>, ml::Int<1>,
              ml::Int<6>, ml::Int<4>, ml::Int<2>,
              ml::Int<0>>
using Perm =
  ml::f<ml::Unwrap<TaggedPerm>, Extract0>;
static_assert( std::is_same_v<
                Permutation,
                Perm>);
using PermutedTuple =
    std::tuple<double, int, int, char, char, char, char>
using Tupl =
  ml::f<ml::Unwrap<TaggedPerm>, Extract1>;
static_assert( std::is_same_v<
                PermutedTuple,
                Tupl>);
```

To run both `Extract0` and `Extract1` on the `TaggedPermutation` we will use([`ml::ProductMap`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Functional/ProductMap.md)), which takes `N` metafunctions and executes all of them on the input parameter pack `Ts...`. As mentioned, we will `Pipe` the results of `Extract0` to `ml::F<TupleBase>`.

```c++
using ExtractBoth =
        ml::ProductMap<
                       Extract0,
                       Extract1,
                       ml::F<TupleBase>>; // Pipe results of both into here
```

We see that it correctly computes the `TupleBase` instance:

```c++
using TupleBase_ =
  ml::f<ml::Unwrap<TaggedPerm>, ExtractBoth>;

using TB1 = 
  TupleBase<ml::ListT<ml::Int<5>, ml::Int<3>, ml::Int<1>, ml::Int<6>, ml::Int<4>,
                      ml::Int<2>, ml::Int<0>>,
            std::tuple<double, int, int, char, char, char, char>>;

static_assert(
        std::is_same_v<
              TB1, TupleBase_>);
```

which matches our wishes from the [`formulated solution`](#formulating-the-solution).

#### MakeBase metafunction

Putting it all together, the `MakeBase` metafunction looks like so:

```c++
template <typename ...Ts>
using MakeBase = ml::f<
    ml::ZipWith<
        Param,
        ml::Sort<ml::Map<ml::Unwrap<ml::Get<1, ml::AlignOf<>>>, ml::Greater<>>,
                 ml::Product<ml::Map<ml::Unwrap<ml::Get<0>>>,
                             ml::Map<ml::Unwrap<ml::Get<1>>, ml::F<std::tuple>>,
                             ml::F<TupleBase>>>>,
    ml::Range<>::f<0, sizeof...(Ts)>, ml::ListT<Ts...>>;
```

#### Computing the inverse permutation of an index `I`

The last thing to do, is to compute the inverse permutation. Each index `I` is inverted by finding position in the permutation.

[**CppML**](https://github.com/ZigaSajovic/CppML) provides the metafunction [`ml::FindIdIf`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/Algorithm/FindIdIf.md)`<Predicate>`, which returns the index of the first element that satisfies the predicate. Hence, we only need to form the predicate. We do so, by **partially evaluating** the [`ml::IsSame`](https://github.com/ZigaSajovic/CppML/blob/master/docs/reference/TypeTraits/IsSame.md) metafunction. For example,

```c++
using Is1 = ml::Partial<ml::IsSame<>, ml::Int<1>>;
using T = ml::f<Is1, ml::Int<2>>;
static_assert(
        std::is_same_v<T, ml::Bool<false>>);
```

This means  that we can invert the index `I` by

```c++
template <typename I>
using Invert = ml::f<
                ml::Unwrap<Permutation>,
                ml::FindIdIf<
                      ml::Partial<ml::IsSame<>>, I>>;
```

### The Tuple wrapper class

With the **permutation** and its **inverse** computed, and the permuted elements extracted, we turn to coding the class interface indirection. The class **Tuple** will contain:

* the permuted `std::tuple` member `_tuple`
  * from its second template argument
* `Invert` alias which will compute the inverse permutation for an index `I`
* a delegate constructor:
  * It will forward the arguments `Us...` as a tuple to the `work construcotr`
* a `work constructor`:
  * it will initialize the `_tuple` member by:
    * permuting the arguments of the forwarding tuple into its initializer
      * `std::get<Is>(fwd)...` 
* the `get<I>()` friend function, which:
  * will use the `f` alias to invert `I` in the `Permutation`
  * and forward the inverted index to `std::get`

#### Code

```c++
template <int... Is, typename... Ts>
struct TupleBase<ml::ListT<ml::Int<Is>...>, std::tuple<Ts...>> {
private:
  std::tuple<Ts...> _tuple;
  template <typename... Us>
  TupleBase(ml::_, std::tuple<Us...> &&fwd) // work constructor
      : _tuple{
            static_cast<ml::f<ml::Get<Is>, Us...> &&>(std::get<Is>(fwd))...} {}

public:
  template <typename... Us>
  TupleBase(Us &&... us) // delegate constructor
      : TupleBase{ml::_{}, std::forward_as_tuple(static_cast<Us &&>(us)...)} {}
  template <typename I> // Compute the inverse index
  using f = ml::f<ml::FindIdIf<ml::Partial<ml::IsSame<>, I>>, ml::Int<Is>...>;
  template <int I, typename... Us>
  friend decltype(auto) get(TupleBase<Us...> &tup);
};

template <int I, typename... Us> decltype(auto) get(TupleBase<Us...> &tup) {
  return std::get<ml::f<TupleBase<Us...>, ml::Int<I>>::value>(tup._tuple);
}
```

Which allows us to implement the `Tuple` class, like so:

```c++
template <typename... Ts> struct Tuple : MakeBase<Ts...> {
  using MakeBase<Ts...>::MakeBase;
};
```

#### Implementing other functions and methods

As `get<N>` is the driving force behind the interface, in terms of which other functionalities are implemented, their implementation is trivial. Here we demonstrate the implementation of the equality operator.

```c++
template <int... Is, template <class...> class T, typename... Ts,
          template <class...> class U, typename... Us, typename F>
decltype(auto) envoker(ml::ListT<ml::Int<Is>...>, const T<Ts...> &t,
                       const U<Us...> &u, F &&f) {
  using std::get;
  return (... && f(get<Is>(t), get<Is>(u)));
}

template <typename... Ts, typename... Us>
auto operator==(const Tuple<Ts...> &lhs, const Tuple<Us...> &rhs) -> bool {
  using List = ml::TypeRange<>::f<0, sizeof...(Ts)>;
  return envoker(List{}, lhs, rhs, [](auto &&x, auto &&y) { return x == y; });
};

template <typename... Ts, typename... Us>
auto operator==(const std::tuple<Ts...> &lhs, const Tuple<Us...> &rhs) -> bool {
  using List = ml::TypeRange<>::f<0, sizeof...(Ts)>;
  return envoker(List{}, lhs, rhs, [](auto &&x, auto &&y) { return x == y; });
};

template <typename... Ts, typename... Us>
auto operator==(const Tuple<Us...> &lhs, const std::tuple<Ts...> &rhs) -> bool {
  rhs == lhs;
};
```
We trust the reader would be able to implement the missing functions and methods, such as other comparison operators, or converting constructors (for `Tuple` and `std::tuple`).
