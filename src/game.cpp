#include "game.h"

#include <engine/action_binds.h>
#include <engine/config.h>
#include <engine/engine.h>
#include <engine/file.h>
#include <engine/ini.h>
#include <engine/log.h>
#include <engine/sprites.h>

#include <memory.h>
#include <string_stream.h>
#include <temp_allocator.h>

#include <cassert>

#if defined(HAS_LUA) || defined(HAS_LUAJIT)
#include "if_game.h"
#endif

namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game);
void game_state_playing_leave(engine::Engine &engine, Game &game);
void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command);
void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt);
void game_state_playing_render(engine::Engine &engine, Game &game);
void game_state_playing_render_imgui(engine::Engine &engine, Game &game);

using namespace foundation;

Game::Game(Allocator &allocator, const char *config_path)
: allocator(allocator)
, config(nullptr)
, action_binds(nullptr)
, sprites(nullptr)
, app_state(AppState::None)
, game_state(allocator) {
    action_binds = MAKE_NEW(allocator, engine::ActionBinds, allocator, config_path);
    sprites = MAKE_NEW(allocator, engine::Sprites, allocator);

    // Config
    {
        TempAllocator1024 ta;

        string_stream::Buffer buffer(ta);
        if (!engine::file::read(buffer, config_path)) {
            log_fatal("Could not open config file %s", config_path);
        }

        config = ini_load(string_stream::c_str(buffer), nullptr);

        if (!config) {
            log_fatal("Could not parse config file %s", config_path);
        }
    }
}

Game::~Game() {
    MAKE_DELETE(allocator, ActionBinds, action_binds);
    MAKE_DELETE(allocator, Sprites, sprites);

    if (config) {
        ini_destroy(config);
    }
}

void update(engine::Engine &engine, void *game_object, float t, float dt) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);

    switch (game->app_state) {
    case AppState::None: {
        transition(engine, game_object, AppState::Initializing);
        break;
    }
    case AppState::Playing: {
        game_state_playing_update(engine, *game, t, dt);
        break;
    }
    case AppState::Quitting: {
        transition(engine, game_object, AppState::Terminate);
        break;
    }
    default: {
        break;
    }
    }
}

void on_input(engine::Engine &engine, void *game_object, engine::InputCommand &input_command) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);
    assert(game->action_binds != nullptr);

    switch (game->app_state) {
    case AppState::Playing: {
        game_state_playing_on_input(engine, *game, input_command);
        break;
    }
    default: {
        break;
    }
    }
}

void render(engine::Engine &engine, void *game_object) {
    Game *game = static_cast<Game *>(game_object);

    switch (game->app_state) {
    case AppState::Playing: {
        game_state_playing_render(engine, *game);
        break;
    }
    default: {
        break;
    }
    }
}

void render_imgui(engine::Engine &engine, void *game_object) {
    Game *game = static_cast<Game *>(game_object);

    switch (game->app_state) {
    case AppState::Playing: {
        game_state_playing_render_imgui(engine, *game);
        break;
    }
    default: {
        break;
    }
    }
}

void on_shutdown(engine::Engine &engine, void *game_object) {
    transition(engine, game_object, AppState::Quitting);
}

void transition(engine::Engine &engine, void *game_object, AppState app_state) {
    if (!game_object) {
        return;
    }

    Game *game = static_cast<Game *>(game_object);

    if (game->app_state == app_state) {
        return;
    }

    // When leaving an game state
    switch (game->app_state) {
    case AppState::Terminate: {
        return;
    }
    case AppState::Playing: {
        game_state_playing_leave(engine, *game);
        break;
    }
    default:
        break;
    }

    game->app_state = app_state;

    // When entering a new game state
    switch (game->app_state) {
    case AppState::None: {
        break;
    }
    case AppState::Initializing: {
        log_info("Initializing");
        const char *atlas_filename = engine::config::read_property(game->config, "game", "atlas_filename");
        if (!atlas_filename) {
            log_fatal("Invalid config file, missing [game] atlas_filename");
        }

        engine::init_sprites(*game->sprites, atlas_filename);

#if defined(HAS_LUA) || defined(HAS_LUAJIT)
        initialize_lua();
#endif

        transition(engine, game_object, AppState::Playing);
        break;
    }
    case AppState::Playing: {
        log_info("Playing");
        game_state_playing_enter(engine, *game);
        break;
    }
    case AppState::Quitting: {
        log_info("Quitting");
        break;
    }
    case AppState::Terminate: {
        log_info("Terminating");
        engine::terminate(engine);
        break;
    }
    }
}

} // namespace game
