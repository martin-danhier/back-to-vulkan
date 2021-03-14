cmake_minimum_required(VERSION 3.17)
project(the_good_one)

set(CMAKE_CXX_STANDARD 17)

# Register externals
add_subdirectory(external)
add_subdirectory(src)
