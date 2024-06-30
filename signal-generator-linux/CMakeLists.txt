cmake_minimum_required(VERSION 3.20)
project(signal-generator LANGUAGES C VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Wall -Wextra -pedantic -Werror)

set(SOURCE_FILES
    signal-generator.c
)

add_executable(signal-generator ${SOURCE_FILES})
target_link_libraries(signal-generator m)

install(TARGETS signal-generator)
