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

set(LUA_VERSION "LUA" CACHE STRING "Selected Lua version")
set_property(CACHE LUA_VERSION PROPERTY STRINGS LUA51 LUAJIT LUAU)

if(LUA_VERSION STREQUAL "LUA51")
    message(STATUS "Compiling with Lua 5.1")
    set(HAS_LUA True)
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

    list(APPEND SRC_giraffe ${SRC_LUA})
elseif(LUA_VERSION STREQUAL "LUAJIT")
    message(STATUS "Compiling with LuaJIT")
    set(HAS_LUA True)
    find_package(PkgConfig REQUIRED)  
    pkg_check_modules(LuaJIT REQUIRED IMPORTED_TARGET luajit)
elseif(LUA_VERSION STREQUAL "LUAU")
    message(STATUS "Compiling with Luau")
    set(HAS_LUA True)
    set(LUAU_EXTERN_C ON CACHE BOOL "Use extern C for all APIs")
    FetchContent_Declare(
        Luau
        GIT_REPOSITORY https://github.com/luau-lang/luau.git
    )
    FetchContent_MakeAvailable(Luau)
endif()

if (HAS_LUA)
    list(FILTER SRC_giraffe EXCLUDE REGEX "src/game_state_playing.cpp")
    list(APPEND SRC_giraffe
        "src/if_game.h"
        "src/if_game.cpp"
    )
endif()

# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_giraffe}
)

if(LUA_VERSION STREQUAL "LUA51")
    target_sources(${PROJECT_NAME} PRIVATE ${SRC_LUA})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUA51)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/lua-5.1.5/src)

    if (MSVC)
        source_group("Lua" FILES ${SRC_LUA})
    endif()
elseif(LUA_VERSION STREQUAL "LUAJIT")
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LuaJIT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUAJIT)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LuaJIT_INCLUDE_DIRS})

    file(GLOB LUAJIT_HEADERS "${LuaJIT_INCLUDE_DIRS}/*.h")
    target_sources(${PROJECT_NAME} PRIVATE ${LUAJIT_HEADERS})

    if (MSVC)
        source_group("LuaJIT" FILES ${LUAJIT_HEADERS})
    endif()
elseif(LUA_VERSION STREQUAL "LUAU")
    target_link_libraries(${PROJECT_NAME} PRIVATE Luau.Compiler)
    target_link_libraries(${PROJECT_NAME} PRIVATE Luau.VM)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LUAU)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Luau_SOURCE_DIR}/Common/include)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Luau_SOURCE_DIR}/Compiler/include)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Luau_SOURCE_DIR}/VM/include)
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


# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE=1)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)

    if (LIVE_PP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIVE_PP=1 NOMINMAX=1)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi /Gm- /Gy /Gw)
        target_link_options(${PROJECT_NAME} PRIVATE /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF /DEBUG:FULL)
    endif()
endif()
