#include "if_game.h"

#include "game.h"

#include <engine/log.h>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace {
lua_State *L = nullptr;
} // namespace

namespace game {

void initialize_lua() {
    L = luaL_newstate();
    luaL_openlibs(L);

    log_info("Initializing lua");

    int load_status = luaL_loadfile(L, "scripts/main.lua");
    if (load_status) {
        log_fatal("Could not load scripts/main.lua: %s", lua_tostring(L, -1));
    }

    int exec_status = lua_pcall(L, 0, 0, 0);
    if (exec_status) {
        log_fatal("Could not run scripts/main.lua: %s", lua_tostring(L, -1));
    }
}

void game_state_playing_enter(engine::Engine &engine, Game &game) {
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {

}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {

}

void game_state_playing_render(engine::Engine &engine, Game &game) {

}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {

}

} // namespace game

#endif // HAS_LUA || HAS_LUAJIT
