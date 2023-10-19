#include "if_game.h"

#include "game.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "array.h"
#include "memory.h"

#include <engine/engine.h>
#include <engine/log.h>
#include <engine/input.h>
#include <engine/action_binds.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace {
lua_State *L = nullptr;
} // namespace

namespace lua {
using namespace foundation;

// Custom print function that uses engine logging.
static int my_print(lua_State* L) {
    using namespace string_stream;

    TempAllocator128 ta;
    Buffer ss(ta);
    
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {
        if (i > 1) {
            ss << "\t";
        }

        const char *str = lua_tolstring(L, i, NULL);
        
        if (str) {
            ss << str;
        } else {
            if (lua_isboolean(L, i)) {
                ss << (lua_toboolean(L, 1) ? "True" : "False");
            } else if (lua_isuserdata(L, i)) {
                ss << "userdata";
            } else if (lua_iscfunction(L, i)) {
                ss << "c function";
            } else if (lua_islightuserdata(L, i)) {
                ss << "light userdata";
            } else if (lua_isnil(L, i)) {
                ss << "nil";
            } else if (lua_isnone(L, i)) {
                ss << "none";
            } else if (lua_istable(L, i)) {
                ss << "table";
            } else if (lua_isthread(L, i)) {
                ss << "thread";
            } else {
                const char *type_name = luaL_typename(L, i);
                log_fatal("Could not convert argument %s to string", type_name);
            }
        }
    }

    log_debug("[LUA] %s", c_str(ss));
    lua_pop(L, nargs);

    return 0;
}

// Getting around the issue that Lua 5.1 doesn't support uint64_t values.
// Pushes a uint64_t value as a userdata object on the stack.
inline sol::object push_uint64_t_to_lua(lua_State *L, uint64_t val) {
    sol::state_view lua(L);
    uint64_t *stored_val = static_cast<uint64_t *>(lua_newuserdata(lua.lua_state(), sizeof(uint64_t)));
    *stored_val = val;
    return sol::make_object(lua, stored_val);
}

// Getting around the issue that Lua 5.1 doesn't support uint64_t values.
// Gets a userdata from the stack and returns the uint64_t value.
inline uint64_t retrieve_uint64_t_from_lua(const sol::userdata &userdata) {
    const uint64_t *data = static_cast<const uint64_t *>(userdata.pointer());
    return *data;
}

// Run a named global function in Lua with variadic arguments.
template<typename... Args>
inline void fun(const char *function_name, Args&&... args) {
    sol::state_view lua(L);
    sol::protected_function f = lua[function_name];

    if (!f.valid()) {
        log_fatal("[LUA] Invalid function %s", function_name);
    }

    sol::protected_function_result r = f(std::forward<Args>(args)...);

    if (!r.valid()) {
        sol::error err = r;
        log_fatal("[LUA] Error in function %s: %s", function_name, err.what());
    }
}

// Create the Engine module and export all functions, types, and enums that's used in this game.
void init_engine_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table engine = lua["Engine"].get_or_create<sol::table>();

    // input.h
    {
        engine.new_enum("InputType",
            "None", engine::InputType::None,
            "Mouse", engine::InputType::Mouse,
            "Key", engine::InputType::Key,
            "Scroll", engine::InputType::Scroll
        );

        engine.new_enum("MouseAction",
            "None", engine::MouseAction::None,
            "MouseMoved", engine::MouseAction::MouseMoved,
            "MouseTrigger", engine::MouseAction::MouseTrigger
        );

        engine.new_enum("TriggerState",
            "None", engine::TriggerState::None,
            "Pressed", engine::TriggerState::Pressed,
            "Released", engine::TriggerState::Released,
            "Repeated", engine::TriggerState::Repeated
        );

        engine.new_enum("CursorMode",
            "Normal", engine::CursorMode::Normal,
            "Hidden", engine::CursorMode::Hidden
        );

        lua.new_usertype<engine::KeyState>("KeyState",
            "keycode", &engine::KeyState::keycode,
            "trigger_state", &engine::KeyState::trigger_state,
            "shift_state", &engine::KeyState::shift_state,
            "alt_state", &engine::KeyState::alt_state,
            "ctrl_state", &engine::KeyState::ctrl_state
        );

        lua.new_usertype<engine::MouseState>("MouseState",
            "mouse_action", &engine::MouseState::mouse_action,
            "mouse_position", &engine::MouseState::mouse_position,
            "mouse_relative_motion", &engine::MouseState::mouse_relative_motion,
            "mouse_left_state", &engine::MouseState::mouse_left_state,
            "mouse_right_state", &engine::MouseState::mouse_right_state
        );

        lua.new_usertype<engine::ScrollState>("ScrollState",
            "x_offset", &engine::ScrollState::x_offset,
            "y_offset", &engine::ScrollState::y_offset
        );

        lua.new_usertype<engine::InputCommand>("InputCommand",
            "input_type", &engine::InputCommand::input_type,
            "key_state", &engine::InputCommand::key_state,
            "mouse_state", &engine::InputCommand::mouse_state,
            "scroll_state", &engine::InputCommand::scroll_state
        );
    }

    // action_binds.h
    {
        sol::table action_binds_table = engine["ActionBindsBind"].get_or_create<sol::table>();
        for (int i = static_cast<int>(engine::ActionBindsBind::FIRST) + 1; i < static_cast<int>(engine::ActionBindsBind::LAST); ++i) {
            engine::ActionBindsBind bind = static_cast<engine::ActionBindsBind>(i);
            action_binds_table[engine::bind_descriptor(bind)] = i;
        }

        engine["bind_descriptor"] = engine::bind_descriptor;
        engine["bind_for_keycode"] = engine::bind_for_keycode;
        engine["action_key_for_input_command"] = [L](const engine::InputCommand &input_command) {
            uint64_t result = engine::action_key_for_input_command(input_command);
            return push_uint64_t_to_lua(L, result);
        };
    }
}

// Create the Game module and export all functions, types, and enums that's used in this game.
void init_game_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table game = lua["Game"].get_or_create<sol::table>();

    sol::table action_hash = game["ActionHash"].get_or_create<sol::table>();
    action_hash["NONE"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::NONE));
    action_hash["QUIT"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::QUIT));
    action_hash["DEBUG_DRAW"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::DEBUG_DRAW));
    action_hash["DEBUG_AVOIDANCE"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::DEBUG_AVOIDANCE));
    action_hash["ADD_ONE"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::ADD_ONE));
    action_hash["ADD_FIVE"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::ADD_FIVE));
    action_hash["ADD_TEN"] = push_uint64_t_to_lua(L, static_cast<uint64_t>(game::ActionHash::ADD_TEN));
}

void initialize() {
    log_info("Initializing lua");

    L = luaL_newstate();
    luaopen_base(L);
    luaL_openlibs(L);

    lua_pushcfunction(L, my_print);
    lua_setglobal(L, "print");

    // Set uint64_t type
    {
        sol::state_view lua(L);

        sol::usertype<uint64_t> uint64_t_usertype = lua.new_usertype<uint64_t>("uint64_t");
        uint64_t_usertype["__eq"] = [](sol::object lhs_obj, sol::object rhs_obj) {
            if (lhs_obj.is<sol::userdata>() && rhs_obj.is<sol::userdata>()) {
                uint64_t lhs = retrieve_uint64_t_from_lua(lhs_obj);
                uint64_t rhs = retrieve_uint64_t_from_lua(rhs_obj);
                return lhs == rhs;
            } else {
                return false;
            }
        };

        lua["push_identifier"] = push_uint64_t_to_lua;
        lua["retrieve_identifier"] = retrieve_uint64_t_from_lua;
        lua["valid_identifier"] = [](sol::userdata userdata) -> bool {
            uint64_t value = retrieve_uint64_t_from_lua(userdata);
            return value != 0;
        };
    }

    init_engine_module(L);
    init_game_module(L);
    
    int load_status = luaL_loadfile(L, "scripts/main.lua");
    if (load_status) {
        log_fatal("Could not load scripts/main.lua: %s", lua_tostring(L, -1));
    }

    int exec_status = lua_pcall(L, 0, 0, 0);
    if (exec_status) {
        log_fatal("Could not run scripts/main.lua: %s", lua_tostring(L, -1));
    }
}

} // namespace lua

// These are the implementation of the functions declared in game.cpp so that all the gameplay implementation runs in Lua.
namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    lua::fun("on_enter", engine, game);
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    lua::fun("on_leave", engine, game);
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    lua::fun("on_input", engine, game, input_command);
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    lua::fun("update", engine, game, t, dt);
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    lua::fun("render", engine, game);
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    lua::fun("render_imgui", engine, game);
}

} // namespace game

#endif // HAS_LUA || HAS_LUAJIT
