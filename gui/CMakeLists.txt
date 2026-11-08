cmake_minimum_required(VERSION 3.16)
project(ActorFrontend LANGUAGES CXX)

# Large IMDb files are much slower to parse without compiler optimization.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

add_executable(actor_gui frontend.cpp)
target_link_libraries(actor_gui PRIVATE Qt6::Widgets Qt6::Concurrent)
