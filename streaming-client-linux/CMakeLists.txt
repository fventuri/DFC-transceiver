cmake_minimum_required(VERSION 3.20)
project(streaming-client LANGUAGES C VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Wall -Wextra -pedantic -Werror)

set(SOURCE_FILES
    streaming-client.c
    clock.c
    dfc.c
    stream.c
    usb.c
)

add_executable(streaming-client ${SOURCE_FILES})
target_link_libraries(streaming-client usb-1.0 m)

install(TARGETS streaming-client)
