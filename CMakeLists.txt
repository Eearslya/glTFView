cmake_minimum_required(VERSION 3.21)
project(glTFView LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin")

add_subdirectory(External)
add_subdirectory(glTFView)
add_subdirectory(Tsuki)
