#include "Tuple.hpp"
#include <iostream>
#include <assert.h>

int main() {

  Tuple<char, int, char, int, char, double, char> tup0{'a', 1,   'c', 3,
                                                       'd', 5.0, 'e'};

  std::cout << sizeof(tup0) << " Bytes" << std::endl;

  std::tuple<char, int, char, int, char, double, char> tup1{'a', 1,   'c', 3,
                                                            'd', 5.0, 'e'};
  std::cout << sizeof(tup1) << " Bytes" << std::endl;

  std::cout << get<2>(tup0) << " == " << std::get<2>(tup1) << std::endl;
  assert(tup0 == tup1);
}
