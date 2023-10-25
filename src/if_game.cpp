#include "if_game.h"

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

#include "game.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "array.h"
#include "memory.h"
#include "hash.h"
#include "util.h"
#include "rnd.h"

#include <engine/sprites.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <engine/input.h>
#include <engine/action_binds.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <engine/math.inl>
#include <engine/atlas.h>
#include <engine/color.inl>

#include <tuple>
#include <array>
#include <stack>
#include <imgui.h>
#include <inttypes.h>

extern "C" {
#if defined(HAS_LUAJIT)
#include <luajit.h>
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>


namespace {
lua_State *L = nullptr;
} // namespace

namespace lua {
using namespace foundation;
using namespace foundation::string_stream;

// Custom print function that uses engine logging.
static int my_print(lua_State *L) {
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

int push_identifier(lua_State* L, uint64_t id) {
    Identifier *identifier = static_cast<lua::Identifier*>(lua_newuserdata(L, sizeof(lua::Identifier)));
    new (identifier) lua::Identifier(id);
    return 1;
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

void init_utilities(lua_State *L) {
    sol::state_view lua(L);
    
    // Wrap uint64_t identifiers for Lua 5.1 that doesn't support 64 bit numbers
    lua.new_usertype<Identifier>("Identifier",
        "valid", &Identifier::valid,
        sol::meta_function::equal_to, [](sol::object lhs_obj, sol::object rhs_obj) {
            if (lhs_obj.is<Identifier>() && rhs_obj.is<Identifier>()) {
                const Identifier &lhs = lhs_obj.as<Identifier>();
                const Identifier &rhs = rhs_obj.as<Identifier>();
                return lhs.value == rhs.value;
            } else {
                return false;
            }
        },
        sol::meta_function::to_string, [L](const Identifier &identifier) {
            TempAllocator64 ta;
            Buffer ss(ta);
            printf(ss, "identifier %llu", identifier.value);
            return sol::make_object(L, c_str(ss));
        }
    );

    lua.new_usertype<rnd_pcg_t>("rnd_pcg_t");
    lua["rnd_pcg_nextf"] = rnd_pcg_nextf;
    lua["rnd_pcg_seed"] = rnd_pcg_seed;
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

void init_imgui_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table imgui = lua["Imgui"].get_or_create<sol::table>();

    imgui["GetForegroundDrawList"] = []() {
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        return draw_list;
    };

    imgui["IM_COL32"] = [](float r, float g, float b, float a) {
        return IM_COL32(r, g, b, a);
    };

    imgui.new_usertype<ImDrawList>("ImDrawList",
        "AddLine", &ImDrawList::AddLine,
        "AddText", [](ImDrawList &drawList, const ImVec2 &pos, ImU32 col, const char *text_begin) {
            drawList.AddText(pos, col, text_begin, nullptr);
        },
        "AddCircle", &ImDrawList::AddCircle
    );

    imgui.new_usertype<ImVec2>("ImVec2",
        sol::constructors<ImVec2(), ImVec2(float, float)>(),
        "x", &ImVec2::x,
        "y", &ImVec2::y
    );
}

void initialize() {
    log_info("Initializing lua");

    L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, my_print);
    lua_setglobal(L, "print");

    init_utilities(L);
    init_engine_module(L);
    init_game_module(L);
    init_foundation_module(L);
    init_glm_module(L);
    init_math_module(L);
    init_imgui_module(L);

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
