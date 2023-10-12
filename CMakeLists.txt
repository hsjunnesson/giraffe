cmake_minimum_required(VERSION 3.21)
project(giraffe
    VERSION 0.0.1
    DESCRIPTION ""
    HOMEPAGE_URL "http://example.com"
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(LIVE_PP False)
set(SUPERLUMINAL False)

# Find locally installed dependencies. Tip: Use VCPKG for these.

if (SUPERLUMINAL)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "c:/Program Files/Superluminal/Performance/API")
    find_package(SuperluminalAPI REQUIRED)
endif()


# Fetch dependencies from Github

include(FetchContent)

FetchContent_Declare(
    BitSquidFoundation
    GIT_REPOSITORY https://github.com/hsjunnesson/bitsquid-foundation.git
)

if (NOT bitsquidfoundation_POPULATED)
    FetchContent_MakeAvailable(BitSquidFoundation)
endif()

add_subdirectory("${CMAKE_SOURCE_DIR}/chocolate")



# Main game source

set(SRC_giraffe
    "src/main.cpp"
    "src/game.h"
    "src/game.cpp"
    "src/game_state_playing.cpp"
)


# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_giraffe}
)


# Includes

if (SUPERLUMINAL)
    include_directories(SYSTEM ${SuperluminalAPI_INCLUDE_DIRS})
endif()

if (LIVE_PP)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/LivePP/API/x64)
endif()


# Linked libraries

target_link_libraries(${PROJECT_NAME} PRIVATE chocolate)

if (SUPERLUMINAL)
    target_link_libraries(${PROJECT_NAME} PRIVATE SuperluminalAPI)
endif()


# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE=1)

    if (LIVE_PP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIVE_PP=1 NOMINMAX=1)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi /Gm- /Gy /Gw)
        target_link_options(${PROJECT_NAME} PRIVATE /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF /DEBUG:FULL)
    endif()

    if (SUPERLUMINAL)
        target_compile_definitions(${PROJECT_NAME} PRIVATE SUPERLUMINAL=1)
    endif()
endif()
