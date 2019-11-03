#include "Tuple.hpp"
#include <assert.h>
#include <iostream>

int main() {

  Tuple<char, int, char, int, char, double, char> tup{'a', 1,   'c', 3,
                                                      'd', 5.0, 'e'};
  std::cout << "Size of out Tuple: " << sizeof(tup) << " Bytes" << std::endl;

  std::tuple<char, int, char, int, char, double, char> std_tup{'a', 1,   'c', 3,
                                                               'd', 5.0, 'e'};
  std::cout << "Size of out std::tuple: " << sizeof(std_tup) << " Bytes"
            << std::endl;

  std::cout << "Actual size of data: "
            << 4 * sizeof(char) + 2 * sizeof(int) + sizeof(double) << " Bytes"
            << std::endl;

  std::cout << get<2>(tup) << " == " << std::get<2>(std_tup) << std::endl;

  std::tuple<char, int, char> t0;
  std::tuple<int> t1;
  std::cout << "Size of t0: " << sizeof(t0) << std::endl;
  std::cout << "Size of t1: " << sizeof(t1) << std::endl;

  assert(tup == std_tup);
}
