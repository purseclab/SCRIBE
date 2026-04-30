cmake_minimum_required(VERSION 3.16)
project(Recompiler)
find_package(LLVM 15 REQUIRED CONFIG)

# https://github.com/llvm/llvm-project/commit/ecfa2d3d9943a48411d04a4b3103c42b4653d9af
# only since LLVM 16 we can use find_package(Clang ${VERSION} REQUIRED CONFIG)
set(Clang_DIR "/lib/cmake/clang-15")
find_package(Clang REQUIRED CONFIG)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "-Wall -fno-rtti") # https://stackoverflow.com/questions/17225956/developing-an-llvm-pass-with-cmake-out-of-llvm-source-directory

add_subdirectory(Passes)
add_subdirectory(Plugins)
