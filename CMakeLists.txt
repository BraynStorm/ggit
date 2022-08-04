cmake_minimum_required(VERSION 3.22)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/run/${CMAKE_BUILD_TYPE}")

project(ggit C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED 23)

include_directories(${CMAKE_SOURCE_DIR}/deps/include)
link_directories(${CMAKE_SOURCE_DIR}/deps/lib)

add_executable(ggit ggit.c)
target_link_libraries(ggit SDL2 SDL2main)

file(
    COPY "${CMAKE_SOURCE_DIR}/deps/bin/SDL2.dll"
    DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)