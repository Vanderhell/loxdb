cmake_minimum_required(VERSION 3.16)
project(loxdb_installed_consumer_c LANGUAGES C)

find_package(loxdb CONFIG REQUIRED)

add_executable(loxdb_installed_consumer_c main.c)
set_target_properties(loxdb_installed_consumer_c PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_link_libraries(loxdb_installed_consumer_c PRIVATE loxdb::loxdb)
