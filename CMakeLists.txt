cmake_minimum_required(VERSION 3.16)
project(lifetree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lifetree
  src/lifetree.cpp
)
target_include_directories(lifetree PUBLIC include)

if(MSVC)
  target_compile_options(lifetree PRIVATE /W4 /permissive-)
else()
  target_compile_options(lifetree PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lifetree_demo src/demo.cpp)
target_link_libraries(lifetree_demo PRIVATE lifetree)

add_executable(lifetree_tests tests/lifetree_tests.cpp)
target_link_libraries(lifetree_tests PRIVATE lifetree)

enable_testing()
add_test(NAME lifetree_tests COMMAND lifetree_tests)
