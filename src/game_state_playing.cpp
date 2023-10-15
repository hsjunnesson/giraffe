#include "game.h"

#include "rnd.h"
#include "util.h"

#include <engine/action_binds.h>
#include <engine/atlas.h>
#include <engine/color.inl>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/math.inl>
#include <engine/sprites.h>

#include <hash.h>
#include <murmur_hash.h>
#include <string_stream.h>
#include <temp_allocator.h>

#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <imgui.h>
#include <time.h>

namespace {
rnd_pcg_t random_device;
const int32_t z_layer = -1;
bool debug_draw = false;
} // namespace

namespace game {
using namespace math;
using namespace foundation;

/// Utility to add a sprite to the game.
uint64_t add_sprite(engine::Sprites &sprites, const char *sprite_name, const int32_t x, const int32_t y, Color4f color = engine::color::white) {
    const engine::Sprite sprite = engine::add_sprite(sprites, sprite_name);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, {x, y, z_layer});
    transform = glm::scale(transform, glm::vec3(sprite.atlas_rect->size.x, sprite.atlas_rect->size.y, 1));
    engine::transform_sprite(sprites, sprite.id, Matrix4f(glm::value_ptr(transform)));
    engine::color_sprite(sprites, sprite.id, color);

    return sprite.id;
}

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;

    time_t seconds;
    time(&seconds);
    rnd_pcg_seed(&random_device, (RND_U32)seconds);

    // Spawn obstacles
    for (int i = 0; i < 8; ++i) {
        Obstacle obstacle;
        obstacle.position = {
            100.0f + rnd_pcg_nextf(&random_device) * (engine.window_rect.size.x - 200.0f),
            100.0f + rnd_pcg_nextf(&random_device) * (engine.window_rect.size.y - 200.0f)};
        obstacle.radius = 50.0f + rnd_pcg_nextf(&random_device) * 100.0f;

        array::push_back(game.obstacles, obstacle);
    }

    // Spawn giraffes outside obstacles
    const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
    assert(giraffe_frame);

    for (int i = 0; i < 10; ++i) {
        Mob giraffe;
        giraffe.mass = 50.0f;
        giraffe.max_force = 1000.0f;
        giraffe.max_speed = 300.0f;
        giraffe.radius = 20.0;

        int attempts = 0;

        bool is_valid = false;
        while (!is_valid) {
            ++attempts;
            if (attempts > 1000) {
                log_fatal("Could not spawn giraffes");
            }

            giraffe.position = {
                50.0f + rnd_pcg_nextf(&random_device) * (engine.window_rect.size.x - 100.0f),
                50.0f + rnd_pcg_nextf(&random_device) * (engine.window_rect.size.y - 100.0f)};

            glm::vec2 pos = giraffe.position + glm::vec2(
                                                   giraffe_frame->rect.size.x * giraffe_frame->pivot.x,
                                                   giraffe_frame->rect.size.y * (1.0f - giraffe_frame->pivot.y));

            is_valid = true;

            for (Obstacle *obstacle = array::begin(game.obstacles); obstacle != array::end(game.obstacles); ++obstacle) {
                if (circles_overlap(pos, obstacle->position, giraffe.radius, obstacle->radius)) {
                    is_valid = false;
                    break;
                }
            }
        }

        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "giraffe", engine::color::pico8::orange);
        giraffe.sprite_id = sprite.id;
        array::push_back(game.mobs, giraffe);
    }
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    if (input_command.input_type == engine::InputType::Key) {
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
        case ActionHash::DEBUG_DRAW: {
            if (pressed) {
                debug_draw = !debug_draw;
            }
            break;
        }
        }
    } else if (input_command.input_type == engine::InputType::Mouse) {
        if (input_command.mouse_state.mouse_left_state == engine::TriggerState::Pressed) {
            game.arrival_position = {input_command.mouse_state.mouse_position.x, engine.window_rect.size.y - input_command.mouse_state.mouse_position.y};
        }
    }
}

void update_mobs(Game &game, float dt) {
    for (Mob *mob = array::begin(game.mobs); mob != array::end(game.mobs); ++mob) {
        switch (mob->type) {
        case Mob::Type::Giraffe: {
            // arrival
            glm::vec2 target_offset = game.arrival_position - mob->position;
            float distance = glm::length(target_offset);
            float ramped_speed = mob->max_speed * (distance / 100.0f);
            float clipped_speed = std::min(ramped_speed, mob->max_speed);
            glm::vec2 desired_velocity = (clipped_speed / distance) * target_offset;
            mob->steering_direction = desired_velocity - mob->velocity;

            break;
        }
        case Mob::Type::Lion: {
            break;
        }
        }
    }
}

void update_locomotion(Game &game, float dt) {
    const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
    assert(giraffe_frame);

    const engine::AtlasFrame *lion_frame = engine::atlas_frame(*game.sprites->atlas, "lion");
    assert(lion_frame);

    float drag = 1.0f;

    for (Mob *mob = array::begin(game.mobs); mob != array::end(game.mobs); ++mob) {
        glm::vec2 drag_force = -drag * mob->velocity;
        glm::vec2 steering_force = truncate(mob->steering_direction, mob->max_force) + drag_force;
        glm::vec2 acceleration = (steering_force / mob->mass);

        mob->velocity = truncate(mob->velocity + acceleration, mob->max_speed);
        mob->position = mob->position + mob->velocity * dt;

        if (glm::length(mob->velocity) > 0.001f) {
            mob->orientation = atan2f(-mob->velocity.x, mob->velocity.y);
        }

        glm::mat4 transform = glm::mat4(1.0f);

        switch (mob->type) {
        case Mob::Type::Giraffe: {
            bool flip = mob->velocity.x <= 0.0f;
            float x_offset = giraffe_frame->rect.size.x * giraffe_frame->pivot.x;
            if (flip) {
                x_offset *= -1.0f;
            }
            transform = glm::translate(transform, glm::vec3(
                                                      floorf(mob->position.x - x_offset),
                                                      floorf(mob->position.y - giraffe_frame->rect.size.y * (1.0f - giraffe_frame->pivot.y)),
                                                      z_layer));
            transform = glm::scale(transform, {(flip ? -1.0f : 1.0f) * giraffe_frame->rect.size.x, giraffe_frame->rect.size.y, 1.0f});
            break;
        }
        case Mob::Type::Lion: {
            bool flip = mob->velocity.x <= 0.0f;
            float x_offset = lion_frame->rect.size.x * lion_frame->pivot.x;
            if (flip) {
                x_offset *= -1.0f;
            }
            transform = glm::translate(transform, glm::vec3(
                                                      floorf(mob->position.x - x_offset),
                                                      floorf(mob->position.y - lion_frame->rect.size.y * (1.0f - lion_frame->pivot.y)),
                                                      z_layer));
            transform = glm::scale(transform, {(flip ? -1.0f : 1.0f) * lion_frame->rect.size.x, lion_frame->rect.size.y, 1.0f});
            break;
        }
        }

        engine::transform_sprite(*game.sprites, mob->sprite_id, Matrix4f(glm::value_ptr(transform)));
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;

    update_mobs(game, dt);
    update_locomotion(game, dt);
    engine::update_sprites(*game.sprites, t, dt);
    engine::commit_sprites(*game.sprites);
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    engine::render_sprites(engine, *game.sprites);
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;

    ImDrawList *draw_list = ImGui::GetForegroundDrawList();

    if (debug_draw) {
        TempAllocator128 ta;

        const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
        const engine::AtlasFrame *lion_frame = engine::atlas_frame(*game.sprites->atlas, "lion");
        assert(giraffe_frame);
        assert(lion_frame);

        for (Mob *mob = array::begin(game.mobs); mob != array::end(game.mobs); ++mob) {
            glm::vec2 origin = {mob->position.x, engine.window_rect.size.y - mob->position.y};

            Buffer ss(ta);

            // steer
            {
                glm::vec2 steer = truncate(mob->steering_direction, mob->max_force);
                steer.y *= -1;
                float steer_ratio = glm::length(steer) / mob->max_force;
                glm::vec2 end = origin + glm::normalize(steer) * (steer_ratio * 50.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(255, 0, 0, 255));
                string_stream::printf(ss, "steer: %.0f", glm::length(steer));
                draw_list->AddText(ImVec2(origin.x, origin.y + 8), IM_COL32(255, 0, 0, 255), string_stream::c_str(ss));
            }

            // velocity
            {
                glm::vec2 vel = truncate(mob->velocity, mob->max_speed);
                vel.y *= -1;
                float vel_ratio = glm::length(vel) / mob->max_speed;
                glm::vec2 end = origin + glm::normalize(vel) * (vel_ratio * 50.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(0, 255, 0, 255));
                array::clear(ss);
                string_stream::printf(ss, "veloc: %.0f", glm::length(vel));
                draw_list->AddText(ImVec2(origin.x, origin.y + 20), IM_COL32(0, 255, 0, 255), string_stream::c_str(ss));
            }

            // radius
            {
                draw_list->AddCircle(ImVec2(origin.x, origin.y), mob->radius, IM_COL32(255, 255, 0, 255));
            }
        }

        // food
        draw_list->AddCircleFilled(ImVec2(game.arrival_position.x, engine.window_rect.size.y - game.arrival_position.y), 3, IM_COL32(255, 255, 0, 255));
        draw_list->AddText(ImVec2(game.arrival_position.x, engine.window_rect.size.y - game.arrival_position.y + 8), IM_COL32(255, 255, 0, 255), "food");
    }

    // obstacles
    static ImU32 obstacle_color = IM_COL32(engine::color::pico8::blue.r * 255.0f, engine::color::pico8::blue.g * 255.0f, engine::color::pico8::blue.b * 255.0f, 255);
    for (Obstacle *obstacle = array::begin(game.obstacles); obstacle != array::end(game.obstacles); ++obstacle) {
        ImVec2 position = ImVec2(obstacle->position.x, engine.window_rect.size.y - obstacle->position.y);
        draw_list->AddCircle(position, obstacle->radius, obstacle_color, 0, 2.0f);
    }
}

} // namespace game
