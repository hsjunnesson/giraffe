cmake_minimum_required(VERSION 3.20)
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
set(SUPERLUMINAL True)

# Compile for Lua 5.1
set(HAS_LUA True)

# Compile for LuaJIT
set(HAS_LUAJIT False)

if (HAS_LUA AND HAS_LUAJIT)
    message(FATAL_ERROR "Can't compile with both HAS_LUA and HAS_LUAJIT")
endif()

if (SUPERLUMINAL)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "c:/Program Files/Superluminal/Performance/API")
    find_package(SuperluminalAPI REQUIRED)
endif()


# Find locally installed dependencies. Tip: Use VCPKG for these.

if (HAS_LUA OR HAS_LUAJIT)
    find_package(sol2 CONFIG REQUIRED)
endif()


# Fetch dependencies from Github

include(FetchContent)

FetchContent_Declare(
    BitSquidFoundation
    GIT_REPOSITORY https://github.com/hsjunnesson/bitsquid-foundation.git
)
FetchContent_MakeAvailable(BitSquidFoundation)

add_subdirectory("${CMAKE_SOURCE_DIR}/chocolate")


# Main game source

set(SRC_giraffe
    "src/main.cpp"
    "src/game.h"
    "src/game.cpp"
    "src/game_state_playing.cpp"
    "src/rnd.h"
    "src/util.h"
)

if (HAS_LUA OR HAS_LUAJIT)
    list(FILTER SRC_giraffe EXCLUDE REGEX "src/game_state_playing.cpp")
endif()

# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_giraffe}
)

if (HAS_LUA OR HAS_LUAJIT)
    set(SRC_interface
        "src/if_game.h"
        "src/if_game.cpp"
    )

    target_sources(${PROJECT_NAME} PRIVATE ${SRC_interface})
endif()

if (HAS_LUA)
    set(SRC_LUA
        "lua-5.1.5/src/lapi.c"
        "lua-5.1.5/src/lauxlib.c"
        "lua-5.1.5/src/lbaselib.c"
        "lua-5.1.5/src/lcode.c"
        "lua-5.1.5/src/ldblib.c"
        "lua-5.1.5/src/ldebug.c"
        "lua-5.1.5/src/ldo.c"
        "lua-5.1.5/src/ldump.c"
        "lua-5.1.5/src/lfunc.c"
        "lua-5.1.5/src/lgc.c"
        "lua-5.1.5/src/linit.c"
        "lua-5.1.5/src/liolib.c"
        "lua-5.1.5/src/llex.c"
        "lua-5.1.5/src/lmathlib.c"
        "lua-5.1.5/src/lmem.c"
        "lua-5.1.5/src/loadlib.c"
        "lua-5.1.5/src/lobject.c"
        "lua-5.1.5/src/lopcodes.c"
        "lua-5.1.5/src/loslib.c"
        "lua-5.1.5/src/lparser.c"
        "lua-5.1.5/src/lstate.c"
        "lua-5.1.5/src/lstring.c"
        "lua-5.1.5/src/lstrlib.c"
        "lua-5.1.5/src/ltable.c"
        "lua-5.1.5/src/ltablib.c"
        "lua-5.1.5/src/ltm.c"
        "lua-5.1.5/src/lundump.c"
        "lua-5.1.5/src/lvm.c"
        "lua-5.1.5/src/lzio.c"
        "lua-5.1.5/src/print.c"
        "lua-5.1.5/src/lapi.h"
        "lua-5.1.5/src/lauxlib.h"
        "lua-5.1.5/src/lcode.h"
        "lua-5.1.5/src/ldebug.h"
        "lua-5.1.5/src/ldo.h"
        "lua-5.1.5/src/lfunc.h"
        "lua-5.1.5/src/lgc.h"
        "lua-5.1.5/src/llex.h"
        "lua-5.1.5/src/llimits.h"
        "lua-5.1.5/src/lmem.h"
        "lua-5.1.5/src/lobject.h"
        "lua-5.1.5/src/lopcodes.h"
        "lua-5.1.5/src/lparser.h"
        "lua-5.1.5/src/lstate.h"
        "lua-5.1.5/src/lstring.h"
        "lua-5.1.5/src/ltable.h"
        "lua-5.1.5/src/ltm.h"
        "lua-5.1.5/src/lua.h"
        "lua-5.1.5/src/luaconf.h"
        "lua-5.1.5/src/lualib.h"
        "lua-5.1.5/src/lundump.h"
        "lua-5.1.5/src/lvm.h"
        "lua-5.1.5/src/lzio.h"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${SRC_LUA})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUA)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.1.5/src)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE chocolate)


# Third party

if (SUPERLUMINAL)
    include_directories(SYSTEM ${SuperluminalAPI_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE SuperluminalAPI)
endif()

if (LIVE_PP)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/LivePP/API/x64)
endif()

if (HAS_LUA OR HAS_LUAJIT)
    target_link_libraries(${PROJECT_NAME} PRIVATE sol2)
endif()

if (HAS_LUAJIT)
    find_package(PkgConfig REQUIRED)  
    pkg_check_modules(LuaJIT REQUIRED IMPORTED_TARGET luajit)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LuaJIT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUAJIT)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LuaJIT_INCLUDE_DIRS})

    file(GLOB LUAJIT_HEADERS "${LuaJIT_INCLUDE_DIRS}/*.h")
    source_group("LuaJIT" FILES ${LUAJIT_HEADERS})
    target_sources(${PROJECT_NAME} PRIVATE ${LUAJIT_HEADERS})
endif()


# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE=1)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)

    if (HAS_LUA)
        source_group("lua" FILES ${SRC_LUA})
    endif()

    if (LIVE_PP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIVE_PP=1 NOMINMAX=1)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi /Gm- /Gy /Gw)
        target_link_options(${PROJECT_NAME} PRIVATE /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF /DEBUG:FULL)
    endif()
endif()
