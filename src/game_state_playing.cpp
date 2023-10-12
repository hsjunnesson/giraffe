#include "game.h"

#include <engine/action_binds.h>
#include <engine/input.h>
#include <hash.h>
#include <murmur_hash.h>

namespace game {
using namespace foundation;

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    (void)game;

    bool pressed = input_command.key_state.trigger_state == engine::TriggerState::Pressed;
    bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

    uint64_t bind_action_key = action_key_for_input_command(input_command);
    if (bind_action_key == 0) {
        return;
    }

    ActionHash action_hash = ActionHash(hash::get(game.action_binds->bind_actions, bind_action_key, (uint64_t)0));

    switch (action_hash) {
    case ActionHash::NONE: {
        break;
    }
    case ActionHash::QUIT: {
        if (pressed) {
            transition(engine, &game, AppState::Quitting);
        }
        break;
    }
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;
    (void)game;
    (void)t;
    (void)dt;
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

} // namespace game
