cmake_minimum_required(VERSION 2.8)


#SET(CMAKE_CXX_COMPILER /usr/bin/clang++)
add_subdirectory(CppML)
add_executable(main MACOSX_BUNDLE main.cpp)
target_link_libraries(main CppML::CppML)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(OPTIMIZATION_LEVEL "-O3")
set(COMPILER_WARNINGS "-Wall -Wextra -Winline -Wshadow")
set(CMAKE_CXX_FLAGS "${OPTIMIZATION_LEVEL} ${COMPILER_WARNINGS}")

target_compile_features(main PRIVATE cxx_std_17)
