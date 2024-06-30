cmake_minimum_required(VERSION 3.20)
project(streaming-client LANGUAGES CXX C VERSION 0.0.1)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

#add_compile_options(-Wall -Wextra -pedantic -Werror)

set(SOURCE_FILES
    getopt.c
    streaming-client.cpp
)

add_executable(streaming-client ${SOURCE_FILES})
target_include_directories(streaming-client PRIVATE "$ENV{FX3_INSTALL_PATH}/library/cpp/inc/")
if(MSVC)
  target_link_libraries(streaming-client "$ENV{FX3_INSTALL_PATH}/library/cpp/lib/x64/CyAPI.lib" setupapi.lib legacy_stdio_definitions.lib)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt.lib")
endif()

install(TARGETS streaming-client)
