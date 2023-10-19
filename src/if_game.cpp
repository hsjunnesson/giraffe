#include "if_game.h"

#include "game.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "array.h"
#include "memory.h"

#include <engine/log.h>
#include <engine/input.h>
#include <engine/action_binds.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace {
lua_State *L = nullptr;
} // namespace

namespace lua {
using namespace foundation;

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

/*
void init_engine_module(lua_State *L) {
    lua_newtable(L);

    // action_binds.h
    {
        lua_pushstring(L, "action_key_for_input_command");
        lua_pushcfunction(L, l_action_key_for_input_command);
        lua_settable(L, -3);
    }

    // input.h
    {
        {
            lua_pushstring(L, "InputType");
            lua_newtable(L);

            lua_pushstring(L, "None");
            lua_pushinteger(L, static_cast<int>(engine::InputType::None));
            lua_settable(L, -3);

            lua_pushstring(L, "Mouse");
            lua_pushinteger(L, static_cast<int>(engine::InputType::Mouse));
            lua_settable(L, -3);

            lua_pushstring(L, "Key");
            lua_pushinteger(L, static_cast<int>(engine::InputType::Key));
            lua_settable(L, -3);

            lua_pushstring(L, "Scroll");
            lua_pushinteger(L, static_cast<int>(engine::InputType::Scroll));
            lua_settable(L, -3);

            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "MouseAction");
            lua_newtable(L);

            lua_pushstring(L, "None");
            lua_pushinteger(L, static_cast<int>(engine::MouseAction::None));
            lua_settable(L, -3);

            lua_pushstring(L, "MouseMoved");
            lua_pushinteger(L, static_cast<int>(engine::MouseAction::MouseMoved));
            lua_settable(L, -3);

            lua_pushstring(L, "MouseTrigger");
            lua_pushinteger(L, static_cast<int>(engine::MouseAction::MouseTrigger));
            lua_settable(L, -3);

            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "TriggerState");
            lua_newtable(L);

            lua_pushstring(L, "None");
            lua_pushinteger(L, static_cast<int>(engine::TriggerState::None));
            lua_settable(L, -3);

            lua_pushstring(L, "Pressed");
            lua_pushinteger(L, static_cast<int>(engine::TriggerState::Pressed));
            lua_settable(L, -3);

            lua_pushstring(L, "Released");
            lua_pushinteger(L, static_cast<int>(engine::TriggerState::Released));
            lua_settable(L, -3);

            lua_pushstring(L, "Repeated");
            lua_pushinteger(L, static_cast<int>(engine::TriggerState::Repeated));
            lua_settable(L, -3);

            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "CursorMode");
            lua_newtable(L);

            lua_pushstring(L, "Normal");
            lua_pushinteger(L, static_cast<int>(engine::CursorMode::Normal));
            lua_settable(L, -3);

            lua_pushstring(L, "Hidden");
            lua_pushinteger(L, static_cast<int>(engine::CursorMode::Hidden));
            lua_settable(L, -3);

            lua_settable(L, -3);
        }
    }

    lua_setglobal(L, "Engine");
}
*/

void init_engine_module(lua_State *L) {
}

void initialize() {
    log_info("Initializing lua");

    L = luaL_newstate();
    luaopen_base(L);
    luaL_openlibs(L);

    lua_pushcfunction(L, my_print);
    lua_setglobal(L, "print");

    init_engine_module(L);

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

namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    lua_getglobal(L, "on_enter");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        log_fatal("Error in on_enter: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    lua_getglobal(L, "on_leave");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        log_fatal("Error in on_leave: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    lua_getglobal(L, "on_input");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);

    lua_newtable(L);

    lua_pushstring(L, "input_type");
    lua_pushinteger(L, static_cast<int>(input_command.input_type));
    lua_settable(L, -3);

    switch (input_command.input_type) {
    case engine::InputType::Mouse: {
        lua_pushstring(L, "mouse_state");
        lua_newtable(L);

        {
            lua_pushstring(L, "mouse_action");
            lua_pushinteger(L, static_cast<int>(input_command.mouse_state.mouse_action));
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "mouse_position");
            lua_newtable(L);
            lua_pushstring(L, "x");
            lua_pushnumber(L, input_command.mouse_state.mouse_position.x);
            lua_settable(L, -3);
            lua_pushstring(L, "y");
            lua_pushnumber(L, input_command.mouse_state.mouse_position.y);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "mouse_relative_motion");
            lua_newtable(L);
            lua_pushstring(L, "x");
            lua_pushnumber(L, input_command.mouse_state.mouse_relative_motion.x);
            lua_settable(L, -3);
            lua_pushstring(L, "y");
            lua_pushnumber(L, input_command.mouse_state.mouse_relative_motion.y);
            lua_settable(L, -3);
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "mouse_left_state");
            lua_pushinteger(L, static_cast<int>(input_command.mouse_state.mouse_left_state));
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "mouse_right_state");
            lua_pushinteger(L, static_cast<int>(input_command.mouse_state.mouse_right_state));
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        break;
    }
    case engine::InputType::Key: {
        lua_pushstring(L, "key_state");
        lua_newtable(L);

        {
            lua_pushstring(L, "keycode");
            lua_pushinteger(L, static_cast<int>(input_command.key_state.keycode));
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "trigger_state");
            lua_pushinteger(L, static_cast<int>(input_command.key_state.trigger_state));
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "shift_state");
            lua_pushboolean(L, input_command.key_state.shift_state);
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "alt_state");
            lua_pushboolean(L, input_command.key_state.alt_state);
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "ctrl_state");
            lua_pushboolean(L, input_command.key_state.ctrl_state);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        break;
    }
    case engine::InputType::Scroll: {
        lua_pushstring(L, "scroll_state");
        lua_newtable(L);

        {
            lua_pushstring(L, "x_offset");
            lua_pushnumber(L, input_command.scroll_state.x_offset);
            lua_settable(L, -3);
        }

        {
            lua_pushstring(L, "y_offset");
            lua_pushnumber(L, input_command.scroll_state.y_offset);
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
        break;
    }
    }

    if (lua_pcall(L, 3, 0, 0) != 0) {
        log_fatal("Error in on_input: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    lua_getglobal(L, "update");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);
    lua_pushnumber(L, t);
    lua_pushnumber(L, dt);

    if (lua_pcall(L, 4, 0, 0) != 0) {
        log_fatal("Error in update: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    lua_getglobal(L, "render");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        log_fatal("Error in render: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    lua_getglobal(L, "render_imgui");

    lua_pushlightuserdata(L, &engine);
    lua_pushlightuserdata(L, &game);

    if (lua_pcall(L, 2, 0, 0) != 0) {
        log_fatal("Error in render_imgui: %s", lua_tostring(L, -1));
    }
}

} // namespace game

#endif // HAS_LUA || HAS_LUAJIT
