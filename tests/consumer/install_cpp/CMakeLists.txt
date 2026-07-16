cmake_minimum_required(VERSION 3.16)
project(loxdb_installed_consumer_cpp LANGUAGES CXX)

find_package(loxdb CONFIG REQUIRED)

add_executable(loxdb_installed_consumer_cpp main.cpp)
set_target_properties(loxdb_installed_consumer_cpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(loxdb_installed_consumer_cpp PRIVATE loxdb::loxdb)
