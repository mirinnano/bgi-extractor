cmake_minimum_required(VERSION 3.15)
project(bgi-extractor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /O2 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

set(SOURCES
    src/main.cpp
    src/arc.cpp
    src/dsc.cpp
    src/parser.cpp
    src/decompressor.cpp
)

set(HEADERS
    src/arc.hpp
    src/dsc.hpp
    src/parser.hpp
    src/encoding.hpp
    src/presets.hpp
    src/decompressor.hpp
)

add_executable(bgi-extractor ${SOURCES} ${HEADERS})

target_include_directories(bgi-extractor PRIVATE src)

# POSIX iconv linking if necessary
if(APPLE)
    target_link_libraries(bgi-extractor PRIVATE iconv)
elseif(UNIX AND NOT APPLE)
    find_package(Iconv QUIET)
    if(Iconv_FOUND)
        target_link_libraries(bgi-extractor PRIVATE Iconv::Iconv)
    endif()
endif()

install(TARGETS bgi-extractor DESTINATION bin)
