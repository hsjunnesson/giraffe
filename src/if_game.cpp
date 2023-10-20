#include "if_game.h"

#include "game.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "array.h"
#include "memory.h"
#include "hash.h"
#include "util.h"

#include <engine/sprites.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <engine/input.h>
#include <engine/action_binds.h>
#include <glm/glm.hpp>
#include <engine/math.inl>

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
using namespace foundation::string_stream;

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

    log_info("[LUA] %s", c_str(ss));
    lua_pop(L, nargs);

    return 0;
}

struct Identifier {
    uint64_t value;
    
    explicit Identifier(uint64_t value)
    : value(value)
    {}

    bool valid() {
        return value != 0;
    }
};

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

    // engine.h
    {
        engine.new_usertype<engine::Engine>("Engine",
            "window_rect", &engine::Engine::window_rect
        );
    }

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

        engine.new_usertype<engine::KeyState>("KeyState",
            "keycode", &engine::KeyState::keycode,
            "trigger_state", &engine::KeyState::trigger_state,
            "shift_state", &engine::KeyState::shift_state,
            "alt_state", &engine::KeyState::alt_state,
            "ctrl_state", &engine::KeyState::ctrl_state
        );

        engine.new_usertype<engine::MouseState>("MouseState",
            "mouse_action", &engine::MouseState::mouse_action,
            "mouse_position", &engine::MouseState::mouse_position,
            "mouse_relative_motion", &engine::MouseState::mouse_relative_motion,
            "mouse_left_state", &engine::MouseState::mouse_left_state,
            "mouse_right_state", &engine::MouseState::mouse_right_state
        );

        engine.new_usertype<engine::ScrollState>("ScrollState",
            "x_offset", &engine::ScrollState::x_offset,
            "y_offset", &engine::ScrollState::y_offset
        );

        engine.new_usertype<engine::InputCommand>("InputCommand",
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

        engine.new_usertype<engine::ActionBinds>("ActionBinds",
            "bind_actions", &engine::ActionBinds::bind_actions
        );

        engine["bind_descriptor"] = engine::bind_descriptor;
        engine["bind_for_keycode"] = engine::bind_for_keycode;
        engine["action_key_for_input_command"] = [L](const engine::InputCommand &input_command) {
            uint64_t result = engine::action_key_for_input_command(input_command);
            return Identifier(result);
        };
    }
}

// Create the Game module and export all functions, types, and enums that's used in this game.
void init_game_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table game = lua["Game"].get_or_create<sol::table>();

    // ActionHash
    {
        sol::table action_hash = game["ActionHash"].get_or_create<sol::table>();
        action_hash["NONE"] =            Identifier(static_cast<uint64_t>(game::ActionHash::NONE));
        action_hash["QUIT"] =            Identifier(static_cast<uint64_t>(game::ActionHash::QUIT));
        action_hash["DEBUG_DRAW"] =      Identifier(static_cast<uint64_t>(game::ActionHash::DEBUG_DRAW));
        action_hash["DEBUG_AVOIDANCE"] = Identifier(static_cast<uint64_t>(game::ActionHash::DEBUG_AVOIDANCE));
        action_hash["ADD_ONE"] =         Identifier(static_cast<uint64_t>(game::ActionHash::ADD_ONE));
        action_hash["ADD_FIVE"] =        Identifier(static_cast<uint64_t>(game::ActionHash::ADD_FIVE));
        action_hash["ADD_TEN"] =         Identifier(static_cast<uint64_t>(game::ActionHash::ADD_TEN));

    }

    // AppState
    {
        sol::table app_state = game["AppState"].get_or_create<sol::table>();
        app_state["None"] =         Identifier(static_cast<uint64_t>(game::AppState::None));
        app_state["Initializing"] = Identifier(static_cast<uint64_t>(game::AppState::Initializing));
        app_state["Playing"] =      Identifier(static_cast<uint64_t>(game::AppState::Playing));
        app_state["Quitting"] =     Identifier(static_cast<uint64_t>(game::AppState::Quitting));
        app_state["Terminate"] =    Identifier(static_cast<uint64_t>(game::AppState::Terminate));
    }

    game.new_usertype<game::Game>("Game",
        "action_binds", &game::Game::action_binds,
        "sprites", &game::Game::sprites
    );

    game["transition"] = [](engine::Engine &engine, game::Game &game, Identifier app_state_identifier) {
        game::AppState app_state = static_cast<game::AppState>(app_state_identifier.value);
        game::transition(engine, &game, app_state);
    };
}

void init_foundation_module(lua_State *L) {
    sol::state_view lua(L);

    // hash.h
    {
        sol::table hash = lua["Hash"].get_or_create<sol::table>();

        // Specific getter for Hash<uint64_t> types. Maps of Identifiers, basically.
        hash["get_identifier"] = [](const foundation::Hash<uint64_t> &hash, sol::userdata key, sol::userdata default_value) {
            if (!key.is<Identifier>()) {
                log_fatal("Invalid key in Hash.get_identifier, not an Identifier");
            }

            if (!default_value.is<Identifier>()) {
                log_fatal("Invalid default_value in Hash.get_identifier, not an Identifier");
            }

            const Identifier &key_identifier = key.as<Identifier>();
            const Identifier &default_value_identifier = default_value.as<Identifier>();

            const uint64_t &result = foundation::hash::get(hash, key_identifier.value, default_value_identifier.value);
            return Identifier(result);
        };
    }
}

void init_glm_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table glm = lua["Glm"].get_or_create<sol::table>();

    glm.new_usertype<glm::vec2>("vec2",
        sol::constructors<glm::vec2(), glm::vec2(float, float)>(),
        "x", &glm::vec2::x,
        "y", &glm::vec2::y,
        sol::meta_function::to_string, [L](const glm::vec2 &v) {
            TempAllocator64 ta;
            Buffer ss(ta);
            printf(ss, "vec2(%f, %f)", v.x, v.y);
            return sol::make_object(L, c_str(ss));
        }
    );
}

void init_math_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table math = lua["Math"].get_or_create<sol::table>();

    math.new_usertype<math::Vector2>("Vector2",
        "x", &math::Vector2::x,
        "y", &math::Vector2::y
    );

    math.new_usertype<math::Vector2f>("Vector2f",
        "x", &math::Vector2f::x,
        "y", &math::Vector2f::y
    );

    math.new_usertype<math::Rect>("Rect",
        "origin", &math::Rect::origin,
        "size", &math::Rect::size
    );

    math.new_usertype<math::Color4f>("Color4f",
        "r", &math::Color4f::r,
        "g", &math::Color4f::g,
        "b", &math::Color4f::b,
        "a", &math::Color4f::a
    );
}

void initialize() {
    log_info("Initializing lua");

    L = luaL_newstate();
    luaopen_base(L);
    luaL_openlibs(L);

    lua_pushcfunction(L, my_print);
    lua_setglobal(L, "print");

    // Wrap uint64_t identifiers for Lua 5.1 that doesn't support 64 bit numbers
    {
        sol::state_view lua(L);

        sol::usertype<Identifier> identifier_type = lua.new_usertype<Identifier>("Identifier",
            "valid", &Identifier::valid
        );

        identifier_type[sol::meta_function::equal_to] = [](sol::object lhs_obj, sol::object rhs_obj) {
            if (lhs_obj.is<Identifier>() && rhs_obj.is<Identifier>()) {
                const Identifier &lhs = lhs_obj.as<Identifier>();
                const Identifier &rhs = rhs_obj.as<Identifier>();
                return lhs.value == rhs.value;
            } else {
                return false;
            }
        };
    }

    init_engine_module(L);
    init_game_module(L);
    init_foundation_module(L);
    init_glm_module(L);
    init_math_module(L);

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
