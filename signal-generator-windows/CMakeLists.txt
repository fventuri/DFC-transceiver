cmake_minimum_required(VERSION 3.20)
project(signal-generator LANGUAGES CXX C VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

#add_compile_options(-Wall -Wextra -pedantic -Werror)

set(SOURCE_FILES
    getopt.c
    signal-generator.cpp
)

add_executable(signal-generator ${SOURCE_FILES})
