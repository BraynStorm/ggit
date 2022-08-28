cmake_minimum_required(VERSION 3.22)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/run/${CMAKE_BUILD_TYPE}")

project(ggit C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED 23)

include_directories(${CMAKE_SOURCE_DIR}/deps/include)
include_directories(${CMAKE_SOURCE_DIR}/deps/small-regex/libsmallregex)
link_directories(${CMAKE_SOURCE_DIR}/deps/lib)

add_executable(
    ggit
    ggit.c
    ggit-vector.c
    ggit-graph.c
    ggit-ui.c
    deps/small-regex/libsmallregex/libsmallregex.c
)
target_link_libraries(ggit SDL2 SDL2main SDL2_ttf)

file(
    COPY
        "${CMAKE_SOURCE_DIR}/deps/bin/SDL2.dll"
        "${CMAKE_SOURCE_DIR}/deps/bin/SDL2_ttf.dll"
    DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

file(
    COPY
        "${CMAKE_SOURCE_DIR}/res/"
    DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/res/"
)