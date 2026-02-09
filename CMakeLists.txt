cmake_minimum_required(VERSION 3.16)
project(wasmedge_dependency_tree_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dependency_tree
  src/dependency_tree.cpp
)
target_include_directories(dependency_tree PUBLIC include)

if(MSVC)
  target_compile_options(dependency_tree PRIVATE /W4 /permissive-)
else()
  target_compile_options(dependency_tree PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(dep_tree_demo src/main.cpp)
target_link_libraries(dep_tree_demo PRIVATE dependency_tree)

add_executable(dep_tree_tests tests/test_dependency_tree.cpp)
target_link_libraries(dep_tree_tests PRIVATE dependency_tree)

enable_testing()
add_test(NAME dep_tree_tests COMMAND dep_tree_tests)

