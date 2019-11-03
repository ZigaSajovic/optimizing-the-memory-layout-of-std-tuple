# Optimizing the memory footprint of std::tuple

In the last few years I have become increasingly interested in bringing higher order concepts of category theory closer to the bits that implement their instances. This leads one to languages like *C++*, where the types have insight into the hardware, which gives the constructs control over how they are mapped onto it. On the way towards of such meta-endeavours I created **CppML**, a [metalanguage for C++](https://github.com/ZigaSajovic/CppML), which I use when developing libraries.
In this text, we will use it to optimize the memory footprint of *std::tuple*, at no runtime or cognitive cost on the end of the user.

Before we begin, have a look at the result.

```c++

  Tuple<char, int, char, int, char, double, char> tup{'a', 1,   'c', 3,
                                                      'd', 5.0, 'e'};
  std::cout << "Size of out Tuple: " << sizeof(tup) << " Bytes" << std::endl;

  std::tuple<char, int, char, int, char, double, char> std_tup{'a', 1,   'c', 3,
                                                               'd', 4.0, 'e'};
  std::cout << "Size of out std::tuple: " << sizeof(std_tup) << " Bytes"
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

The solution spans roughly `70` lines of code, which we will build up step by step in this *README*. Please note that it does not contain all the functionalities required of *std::tuple*, but it does provide all the non-trivial implementations (hence others are trivially implementable in terms (or in light) of those provided). The entire code can be found [here](https://github.com/ZigaSajovic/optimizing-the-memory-footprint-of-std-tuple/blob/master/Tuple.hpp).

Note that while this text is not intended as a tutorial on [**CppML**](https://github.com/ZigaSajovic/CppML), we will include explanations and illustrative examples along the way. Please take a look at its [README](https://github.com/ZigaSajovic/CppML), which contains further explanations.

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

Given what we know, we could formulate a solution in two parts. First we optimize the permutation of elements, and memorize it (as a type list, with no runtime trace). We than create a wrapper class *Tuple* that stores an *std::tuple* in the permuted order, and provides a level of (static) indirection when interfacing with it. This is done by having both the **permutation** and its **inverse**. Than, we use the **permutation** to create the layout of the stored tuple, and use the **inverse permutation** to internally map indexes with which the user interfaces with the tuple.

We will first describe the metaprogram that computes the permutations, and than code the interface indirecting wrapper.

### Metaprogram

To get the permutation and its inverse, we take the list of types, enumerate (tag) them (so we can track their position), and sort them by their alignment. We than extract the enumerates from the sequence as our permutation, and compute its inverse.
Below we specify the **metaprogram** in steps, and provide an example execution on the above example of `std::tuple<char, int, char>`.

* Enumerate the list of types
  * Start with two lists:
    * `List1`: list of types:
      * e.g. `ml::ListT<char, int, char>`
    * `List2`: integer sequence of same length
      * e.g. `ml::ListT<ml::Int<0>, ml::Int<1>, ml::Int<2>>`
  * and a `Tag` template with `2` arguments:
    * e.g. `Tag<T, U>`
  * Zip lists `List1` and `List2` with `Tag`:
    * e.g. `ml::ListT<Tag<ml::Int<0>, char>, Tag<ml::Int<1>, char>, Tag<ml::Int<1>, char>>`
* Sort the enumerated list, using the alignment of the second component as key
  * e.g. `ml::ListT<Tag<ml::Int<1>, int>, Tag<ml::Int<0>, char>, Tag<ml::Int<2>, char>>`
* Extract the types, and the permutation:
  * Permutation:
    * `ml::List<ml::Int<1>, ml::Int<2>, ml::Int<0>>`
  * Types:
    * `ml::ListT<int, char, char>`
* Compute the inverse permutation:
  * Inverse Permutation:
    * `ml::ListT<ml::Int<2>, ml::Int<0>, ml::Int<1>>`

#### Enumerating the list of types

Our goal is two-fold. We wish to generate a permutation of elements, which will optimize the object size, while ensuring the **as if** rule. This means that the user is to be oblivious to the permutation behind the scene, and can access elements in the same order he/she originally specified. To this purpose, we will enumerate the types (i.e. **Zip** them with appropriate index) before permuting them.

[**CppML**](https://github.com/ZigaSajovic/CppML) provides the **ZipWith** metafunction, which takes `N` lists of types, and Zips them using the provided template. It also provides  **TypeRange**, which generates a list of integer constants `ml::ListT<ml::Int<0>, ..., ml::Int<N>>`.

```c++
template <typename Id, typename T> struct Tag {};
template <typename... Ts>
using TaggedList =
    ml::Invoke<ml::ZipWith<Tag // Type with which to Zip
                           //, ml::ToList<> // is the implicit Pipe
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

**Note** the implicit **Pipe** (`ml::ToList<>`) in the above code section. **Pipe** is a key concept in [**CppML**](https://github.com/ZigaSajovic/CppML), it is how metafunction execution can be chained (think bash pipes). In the above code section, the implicit **Pipe** is **ml::ToList<>**,which returns the result in a list. We will be replacing it with **Sort** later (i.e. we will pipe the result of **Zip** into **Sort**).

#### Sorting the enumerated type list by the second element

As specified, we will use **Sort** as a greedy solution to the packing problem. **Sort<Predicate>** provided by [**CppML**](https://github.com/ZigaSajovic/CppML) operates on a parameter pack:

```c++
using SortedIs = ml::Invoke<ml::Sort<ml::Greater<>>, // Predicate
                            ml::Int<0>, ml::Int<2>, ml::Int<1>>;
using Sorted = ml::ListT<ml::Int<2>, ml::Int<1>, ml::Int<0>>;
static_assert(std::is_same_v<SortedIs, Sorted>);
```

We need to write a predicate metafunction appropriate for out list of tagged types.

##### Constructing the predicate metafunction

The predicate is a metafunction mapping a pair of types to `ml::Bool<trueth_val>`. The elements of the list are of the form `Tag<ml::Int<I0>, T0>`, and we wish to compare on
**Aligment**s of `T`s. Therefore our predicate is to be a metafunction that maps
```c++
(Tag<ml::Int<I0>, T0>, Tag<ml::Int<I1>, T1>)
->
(T0, T1)
->
(ml::Int<aligment_of_T0>, ml::Int<aligment_of_T1>)
->
ml::Bool<truth_val>
```
We will achieve this by *taking a pack of two types* and  **Apply**ing a metafunction that **UnList**s the `Tag`, *pipes* the result to **Get<1>**, to extract the second element (being `Ti`), and pipe the result to **AligmentOf**. We will than *pipe* the result of **Apply** to **Greater**.
```c++
using Predicate = ml::Apply<        // Apply to each of the two types:
    ml::UnList<                     // Unwrap the Tag
        ml::Get<1,                  // Extract the second element
                ml::AligmentOf<>>>, // Take its alignment
    ml::Greater<>>;                 // And Pipe the result of Apply to
                                    // Greater
```
For example
```c++
static_assert(
  std::is_same_v<
    ml::Invoke<Predicate, Tag<ml::Int<2>, double>, Tag<ml::Int<5>, float>>,
    ml::Bool<true>>)
```
because alignment of `double` is greater than that of `float` (on my machine).

#### Computing the tagged permutation

We can now put it all together.

```c++
template <typename Id, typename T> struct Tag{};
template <typename... Ts>
using TaggedPermutation =
    ml::Invoke<ml::ZipWith<Tag, // Zip the input lists with Tag and pipe into
                           ml::Sort<Predicate>>, // than compare the generated
                                                 // elements (pipe-ing from the
                                                 // Apply) using Greater
               typename ml::TypeRange<>::template f<
                   0, sizeof...(Ts)>, // generate a range from
                                      // [0, numOfTypes)
               ml::ListT<Ts...>>;
```
On a concrete example, this metafunction evaluates to
```c++
using TaggedPerm = TaggedPermutation<char, int, char, int, char, double, char>;
using List =
    ml::ListT<Tag<ml::Const<int, 5>, double>, Tag<ml::Const<int, 3>, int>,
              Tag<ml::Const<int, 1>, int>, Tag<ml::Const<int, 6>, char>,
              Tag<ml::Const<int, 4>, char>, Tag<ml::Const<int, 2>, char>,
              Tag<ml::Const<int, 0>, char>>
static_assert(
              std::is_same_v<TaggedPerm, List>);
```

#### Extracting the permutation and the permuted tuple, from the tagged permutation

Examining the `TaggedPerm` above, it is essentially a list of Tags, `Tag<ml::Const<int, I>, T>`, with `I` being the original positionof `T`. We now need to split this list into two lists, where the first will only hold the integer constants, and the other only the types. We accomplish this by **Apply**ing the **Get<N>** to each `Tag`. Note that we will need to **UnList** the `Tag`, as **Apply** operates on parameter packs.

```c++
template <int N, typename List, typename Pipe = ml::ToList>
using Extract = ml::Invoke<         // Invoke the following metafunction
    ml::UnList<                     // UnList the List into a parameter pack
                                    // and Pipe into
        ml::Apply<                  // Applying
            ml::UnList<ml::Get<N>>, // Get N-th element
            Pipe>>,
    List>;
```
This metafunction can now be used on the computed **TaggedPerm**. **Note** that we can pass the **Pipe** into it. We will do so for *pipeing*  extracted *types* into **ml::F\<std::tuple\>**.
```c++
using Permutation =
    ml::ListT<ml::Const<int, 5>, ml::Const<int, 3>, ml::Const<int, 1>,
              ml::Const<int, 6>, ml::Const<int, 4>, ml::Const<int, 2>,
              ml::Const<int, 0>>
static_assert( std::is_same_v<
                Permutation,
                Extract<0, TaggedPerm>>);
using PermutedTuple =
    std::tuple<double, int, int, char, char, char, char>
static_assert( std::is_same_v<
                Types,
                Extract<1, TaggedPerm, ml::F<std::tuple>>>);
```

#### Computing the inverse permutation

The last thing to do, is to compute the inverse permutation.

The inverse is computed as follows:
  * for each `N`:
    * find the index `I` at which `N` appears in the permutation
    * set `N`-th element to `I`

First we write the metafunction that takes a type `T` and returns the index at which it appears in the `Permutation`. [**CppML**](https://github.com/ZigaSajovic/CppML) provides the **FindIf<Predicate>** metafunction. We need to write a PredicateFactory, which will take `T`, and return a Predicate. We do this by **partially evaluating** the **IsSame** metafunction on `T`.
```c++
template <typename T>
using PredicateFactory = ml::Partial<ml::IsSame<>, T>;
```
PredicateFactory returns a metafunction mapping a single type to `ml::Bool<truth_val>`.

```c++
template <typename T>
using Finder =
      ml::Invoke<
          ml::UnList<
                ml::FindIf< // Get the index of the element satisfying
                            PredicateFactory<T>>>, // the predicate
          Permutation>;
```

To compute the **InversePermutation**, all we need to do is to apply the **Finder** to a sequence `[ml::Int<0>, ..., ml::Int<N>]`. We do this by again constructing the **TypeRange** and pipe it into **ml::WrapIn1<Finder>**.
```c++
using InversePermutation = typename ml::TypeRange<
      ml::Apply<ml::WrapIn1<Finder>>>::template f<0, ml::Length<Permutation>>;
```
A specific index is than inverted, by looking up the element at its index, in the **Inverse Permutation**.
```c++
template <int I>
using Index = ml::Invoke<ml::UnList<ml::Get<I>>, InversePermutation>;
```

### The Tuple wrapper class

With the **permutation** and its **inverse** computed, and the permuted elements extracted, we turn to coding the class interface indirection. The class **Tuple** will contain:

* member tuple:
  * an `std::tuple` with the permuted elements
  * this is the `_Tuple tuple` member
* delegate constructor:
  * it will forward the passed elements as a tuple, and
  * along with an instance of the **permutation** integer sequence
* working constructor:
  * using the passed permutation list it will:
    * unpack the elements from the forwarded tuple in the permuted order
    * it will also perform a static cast to the correct (permuted)
      reference type (akin to `std::forward`)
* a friend `Get<N>(Tuple)` function:
  * it will have access to the  **inverse permutation**,
  * and will remap the index before forwarding to `std::get`

#### Code

```c++
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

  _Tuple _tuple;                       // permuted tuple std::tuple<...>
  template <int... Is, typename... Us> // delegate constructor
  Tuple(ml::ListT<ml::Int<Is>...>, std::tuple<Us...> &&fwd)
      : _tuple{static_cast<ml::Invoke<ml::Get<Is>, Us...> &&>(
            std::get<Is>(fwd))...} {}

public:
  template <typename... Us> // working constructor
  Tuple(Us &&... us)
      : Tuple{Permutation{}, std::forward_as_tuple(static_cast<Us &&>(us)...)} {
  }
  template <int I, typename... Us> friend decltype(auto) get(Tuple<Us...> &tup);
  template <int I, typename... Us>
  friend decltype(auto) get(const Tuple<Us...> &tup);
};

template <int I, typename... Ts> decltype(auto) get(const Tuple<Ts...> &tup) {
  // map the index
  using _I = typename Tuple<Ts...>::template Index<I>;
  return std::get<_I::value>(tup._tuple);
}
template <int I, typename... Ts> decltype(auto) get(Tuple<Ts...> &tup) {
  // map the index
  using _I = typename Tuple<Ts...>::template Index<I>;
  return std::get<_I::value>(tup._tuple);
}
```

#### Implementing other functions and methods

As **get<N>** is the driving force behind the interface, in terms of which other functionalities are implemented, their implementation is trivial. Here we demonstrate the implementation of the equality operator.

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
We trust the reader would be able to implement the missing functions and methods, such as other comparison operators, or converting constructors (for *Tuple* and *std::tuple*).
