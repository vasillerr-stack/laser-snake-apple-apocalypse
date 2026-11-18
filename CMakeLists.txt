cmake_minimum_required(VERSION 3.21)
project(LASER_SNAKE_APPLE_APOCALYPSE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SFML)

add_executable(LASER_SNAKE main.cpp)
set_target_properties(LASER_SNAKE PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
configure_file("${CMAKE_SOURCE_DIR}/assets/DejaVuSansMono.ttf" "${CMAKE_BINARY_DIR}/font.ttf" COPYONLY)
target_link_libraries(LASER_SNAKE PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)

if(WIN32)
    add_custom_command(TARGET LASER_SNAKE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:LASER_SNAKE>
        $<TARGET_FILE_DIR:LASER_SNAKE>
        COMMAND_EXPAND_LISTS
    )
endif()
