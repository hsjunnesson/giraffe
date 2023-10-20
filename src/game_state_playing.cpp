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
#include <limits.h>
#include <time.h>

namespace {
rnd_pcg_t RANDOM_DEVICE;
const int32_t LION_Z_LAYER = -1;
const int32_t GIRAFFE_Z_LAYER = -2;
const int32_t FOOD_Z_LAYER = -3;
} // namespace

namespace game {
using namespace math;
using namespace foundation;

void spawn_giraffes(engine::Engine &engine, Game &game, int num_giraffes) {
    for (int i = 0; i < num_giraffes; ++i) {
        Giraffe giraffe;
        giraffe.mob.mass = 100.0f;
        giraffe.mob.max_force = 1000.0f;
        giraffe.mob.max_speed = 300.0f;
        giraffe.mob.radius = 20.0;
        giraffe.mob.position = {
            50.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.x - 100.0f),
            50.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.y - 100.0f)};

        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "giraffe", engine::color::pico8::orange);
        giraffe.sprite_id = sprite.id;
        array::push_back(game.game_state.giraffes, giraffe);
    }
}

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    time_t seconds;
    time(&seconds);
    rnd_pcg_seed(&RANDOM_DEVICE, (RND_U32)seconds);

    // Spawn lake
    Obstacle obstacle;
    obstacle.position = {
        (engine.window_rect.size.x / 2) + 200.0f * rnd_pcg_nextf(&RANDOM_DEVICE) - 100.0f,
        (engine.window_rect.size.y / 2) + 200.0f * rnd_pcg_nextf(&RANDOM_DEVICE) - 100.0f};
    obstacle.color = engine::color::pico8::blue;
    obstacle.radius = 100.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * 100.0f;

    array::push_back(game.game_state.obstacles, obstacle);

    // Spawn trees
    for (int i = 0; i < 10; ++i) {
        Obstacle obstacle;
        obstacle.position = {
            10.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.x - 20.0f),
            10.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.y - 20.0f)};
        obstacle.color = engine::color::pico8::light_gray;
        obstacle.radius = 20.0f;

        array::push_back(game.game_state.obstacles, obstacle);
    }

    // Spawn giraffes
    spawn_giraffes(engine, game, 3);

    // Spawn food
    {
        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "food", engine::color::pico8::green);
        game.game_state.food.sprite_id = sprite.id;
        game.game_state.food.position = {0.25f * engine.window_rect.size.x, 0.25f * engine.window_rect.size.y};

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(
                                                  floorf(game.game_state.food.position.x - sprite.atlas_frame->rect.size.x * sprite.atlas_frame->pivot.x),
                                                  floorf(game.game_state.food.position.y - sprite.atlas_frame->rect.size.y * (1.0f - sprite.atlas_frame->pivot.y)),
                                                  FOOD_Z_LAYER));
        transform = glm::scale(transform, {sprite.atlas_frame->rect.size.x, sprite.atlas_frame->rect.size.y, 1.0f});
        engine::transform_sprite(*game.sprites, game.game_state.food.sprite_id, Matrix4f(glm::value_ptr(transform)));
    }

    // Spawn lion
    {
        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "lion", engine::color::pico8::yellow);
        game.game_state.lion.sprite_id = sprite.id;
        game.game_state.lion.mob.position = {engine.window_rect.size.x * 0.75f, engine.window_rect.size.y * 0.75f};
        game.game_state.lion.mob.mass = 25.0f;
        game.game_state.lion.mob.max_force = 1000.0f;
        game.game_state.lion.mob.max_speed = 400.0f;
        game.game_state.lion.mob.radius = 20.0;
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
                game.game_state.debug_draw = !game.game_state.debug_draw;
            }
            break;
        }
        case ActionHash::DEBUG_AVOIDANCE: {
            if (pressed) {
                game.game_state.debug_avoidance = !game.game_state.debug_avoidance;
            }
            break;
        }
        case ActionHash::ADD_ONE: {
            if (pressed || repeated) {
                spawn_giraffes(engine, game, 1);
            }
            break;
        }
        case ActionHash::ADD_FIVE: {
            if (pressed || repeated) {
                spawn_giraffes(engine, game, 5);
            }
            break;
        }
        case ActionHash::ADD_TEN: {
            if (pressed || repeated) {
                spawn_giraffes(engine, game, 10);
            }
            break;
        }
        }
    } else if (input_command.input_type == engine::InputType::Mouse) {
        if (input_command.mouse_state.mouse_left_state == engine::TriggerState::Pressed) {
            game.game_state.food.position = {input_command.mouse_state.mouse_position.x, engine.window_rect.size.y - input_command.mouse_state.mouse_position.y};

            const engine::Sprite *sprite = engine::get_sprite(*game.sprites, game.game_state.food.sprite_id);
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                                                      floorf(game.game_state.food.position.x - sprite->atlas_frame->rect.size.x * sprite->atlas_frame->pivot.x),
                                                      floorf(game.game_state.food.position.y - sprite->atlas_frame->rect.size.y * (1.0f - sprite->atlas_frame->pivot.y)),
                                                      FOOD_Z_LAYER));
            transform = glm::scale(transform, {sprite->atlas_frame->rect.size.x, sprite->atlas_frame->rect.size.y, 1.0f});
            engine::transform_sprite(*game.sprites, game.game_state.food.sprite_id, Matrix4f(glm::value_ptr(transform)));
        }
    }
}

void update_mob(Mob &mob, Game &game, float dt) {
    float drag = 1.0f;

    // lake drags you down
    for (Obstacle *obstacle = array::begin(game.game_state.obstacles); obstacle != array::end(game.game_state.obstacles); ++obstacle) {
        const float length = glm::length(obstacle->position - mob.position);
        if (length <= obstacle->radius) {
            drag = 10.0f;
            break;
        }
    }

    const glm::vec2 drag_force = -drag * mob.velocity;
    const glm::vec2 steering_force = truncate(mob.steering_direction, mob.max_force) + drag_force;
    const glm::vec2 acceleration = steering_force / mob.mass;

    mob.velocity = truncate(mob.velocity + acceleration, mob.max_speed);
    mob.position = mob.position + mob.velocity * dt;

    if (glm::length(mob.velocity) > 0.001f) {
        mob.orientation = atan2f(-mob.velocity.x, mob.velocity.y);
    }
}

glm::vec2 arrival_behavior(const Mob &mob, const glm::vec2 target_position, float speed_ramp_distance) {
    const glm::vec2 target_offset = target_position - mob.position;
    const float distance = glm::length(target_offset);
    const float ramped_speed = mob.max_speed * (distance / speed_ramp_distance);
    const float clipped_speed = std::min(ramped_speed, mob.max_speed);
    const glm::vec2 desired_velocity = (clipped_speed / distance) * target_offset;
    return desired_velocity - mob.velocity;
}

glm::vec2 avoidance_behavior(const Mob &mob, engine::Engine &engine, Game &game) {
    const float look_ahead_distance = 200.0f;

    const glm::vec2 origin = mob.position;
    const float target_distance = glm::length(mob.steering_target - origin);
    const glm::vec2 forward = glm::normalize(mob.steering_target - origin);

    const glm::vec2 right_vector = {forward.y, -forward.x};
    const glm::vec2 left_vector = {-forward.y, forward.x};

    const glm::vec2 left_start = origin + left_vector * mob.radius;
    const glm::vec2 right_start = origin + right_vector * mob.radius;

    bool left_intersects = false;
    glm::vec2 left_intersection;
    float left_intersection_distance = FLT_MAX;

    bool right_intersects = false;
    glm::vec2 right_intersection;
    float right_intersection_distance = FLT_MAX;

    // check against each obstacle
    for (Obstacle *obstacle = array::begin(game.game_state.obstacles); obstacle != array::end(game.game_state.obstacles); ++obstacle) {
        glm::vec2 li;
        bool did_li = ray_circle_intersection(left_start, forward, obstacle->position, obstacle->radius, li);
        if (did_li) {
            float distance = glm::length(li - left_start);
            if (distance <= look_ahead_distance && distance < left_intersection_distance) {
                left_intersects = true;
                left_intersection = li;
                left_intersection_distance = distance;
            }
        }

        glm::vec2 ri;
        bool did_ri = ray_circle_intersection(right_start, forward, obstacle->position, obstacle->radius, ri);
        if (did_ri) {
            float distance = glm::length(ri - right_start);
            if (distance <= look_ahead_distance && distance < right_intersection_distance) {
                right_intersects = true;
                right_intersection = li;
                right_intersection_distance = distance;
            }
        }
    }

    glm::vec2 avoidance_force = {0.0f, 0.0f};

    if (right_intersects || left_intersects) {
        if (target_distance <= left_intersection_distance && target_distance <= right_intersection_distance) {
            return {0.0f, 0.0f};
        }

        if (left_intersection_distance < right_intersection_distance) {
            float ratio = left_intersection_distance / look_ahead_distance;
            avoidance_force = right_vector * (1.0f - ratio) * 50.0f;
        } else {
            float ratio = right_intersection_distance / look_ahead_distance;
            avoidance_force = left_vector * (1.0f - ratio) * 50.0f;
        }
    }

    return avoidance_force;
}

void update_giraffe(Giraffe &giraffe, engine::Engine &engine, Game &game, float dt) {
    glm::vec2 arrival_force = {0.0f, 0.0f};
    const float arrival_weight = 1.0f;

    glm::vec2 flee_force = {0.0f, 0.0f};
    float flee_weight = 1.0f;

    glm::vec2 separation_force = {0.0f, 0.0f};
    const float separation_weight = 10.0f;

    glm::vec2 avoidance_force = {0.0f, 0.0f};
    const float avoidance_weight = 10.0f;

    if (!giraffe.dead) {
        bool is_hunted = false;

        float distance = glm::length(game.game_state.lion.mob.position - giraffe.mob.position);
        if (distance <= 300.0f) {
            is_hunted = true;
        }

        // flee
        if (is_hunted) {
            const glm::vec2 flee_direction = giraffe.mob.position - game.game_state.lion.mob.position;
            const glm::vec2 steering_target = giraffe.mob.position + flee_direction;
            giraffe.mob.steering_target = steering_target;

            glm::vec2 desired_velocity = glm::normalize(flee_direction) * giraffe.mob.max_speed;
            flee_force = desired_velocity - giraffe.mob.velocity;

            glm::vec2 boundary_avoidance_force = {0.0f, 0.0f};
            const float buffer_distance = 100.0f;

            if (giraffe.mob.position.x <= buffer_distance) {
                boundary_avoidance_force.x = buffer_distance - giraffe.mob.position.x;
            } else if (giraffe.mob.position.x >= engine.window_rect.size.x - buffer_distance) {
                boundary_avoidance_force.x = engine.window_rect.size.x - buffer_distance - giraffe.mob.position.x;
            }

            if (giraffe.mob.position.y <= buffer_distance) {
                boundary_avoidance_force.y = buffer_distance - giraffe.mob.position.y;
            } else if (giraffe.mob.position.y >= engine.window_rect.size.y - buffer_distance) {
                boundary_avoidance_force.y = engine.window_rect.size.y - buffer_distance - giraffe.mob.position.y;
            }

            boundary_avoidance_force *= 20.0f;

            flee_force += boundary_avoidance_force;
            flee_force *= flee_weight;
        } else {
            // arrival
            giraffe.mob.steering_target = game.game_state.food.position;
            arrival_force = arrival_behavior(giraffe.mob, game.game_state.food.position, 100.0f);
            arrival_force *= arrival_weight;
        }

        // separation
        {
            for (Giraffe *other_giraffe = array::begin(game.game_state.giraffes); other_giraffe != array::end(game.game_state.giraffes); ++other_giraffe) {
                if (other_giraffe != &giraffe && !other_giraffe->dead) {
                    glm::vec2 offset = other_giraffe->mob.position - giraffe.mob.position;
                    const float distance_squared = glm::length2(offset);
                    const float near_distance_squared = (giraffe.mob.radius + other_giraffe->mob.radius) * (giraffe.mob.radius + other_giraffe->mob.radius);
                    if (distance_squared <= near_distance_squared) {
                        float distance = sqrt(distance_squared);
                        offset /= distance;
                        offset *= (giraffe.mob.radius + other_giraffe->mob.radius);
                        separation_force -= offset;
                    }
                }
            }

            separation_force *= separation_weight;
        }

        // avoidance
        {
            avoidance_force = avoidance_behavior(giraffe.mob, engine, game);
            avoidance_force *= avoidance_weight;
        }
    }

    giraffe.mob.steering_direction = truncate(arrival_force + flee_force + separation_force + avoidance_force, giraffe.mob.max_force);

    update_mob(giraffe.mob, game, dt);

    const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
    assert(giraffe_frame);

    glm::mat4 transform = glm::mat4(1.0f);
    bool flip_x = giraffe.mob.velocity.x <= 0.0f;
    bool flip_y = giraffe.dead;

    float x_offset = giraffe_frame->rect.size.x * giraffe_frame->pivot.x;
    float y_offset = giraffe_frame->rect.size.y * (1.0f - giraffe_frame->pivot.y);

    if (flip_x) {
        x_offset *= -1.0f;
    }

    if (flip_y) {
        y_offset *= -1.0f;
    }

    transform = glm::translate(transform, glm::vec3(
                                              floorf(giraffe.mob.position.x - x_offset),
                                              floorf(giraffe.mob.position.y - y_offset),
                                              GIRAFFE_Z_LAYER));
    transform = glm::scale(transform, {(flip_x ? -1.0f : 1.0f) * giraffe_frame->rect.size.x, (flip_y ? -1.0f : 1.0f) * giraffe_frame->rect.size.y, 1.0f});
    engine::transform_sprite(*game.sprites, giraffe.sprite_id, Matrix4f(glm::value_ptr(transform)));
}

void update_lion(Lion &lion, engine::Engine &engine, Game &game, float t, float dt) {
    if (!lion.locked_giraffe) {
        if (lion.energy >= lion.max_energy) {
            Giraffe *found_giraffe = nullptr;
            float distance = FLT_MAX;
            for (Giraffe *giraffe = array::begin(game.game_state.giraffes); giraffe != array::end(game.game_state.giraffes); ++giraffe) {
                if (!giraffe->dead) {
                    float d = glm::length(giraffe->mob.position - lion.mob.position);
                    if (d < distance) {
                        distance = d;
                        found_giraffe = giraffe;
                    }
                }
            }

            if (found_giraffe) {
                lion.locked_giraffe = found_giraffe;
                lion.energy = lion.max_energy;
            }
        } else {
            lion.energy += dt * 2.0f;
        }
    }

    if (lion.locked_giraffe) {
        lion.mob.steering_target = lion.locked_giraffe->mob.position;

        glm::vec2 pursue_force = {0.0f, 0.0f};
        float pursue_weight = 1.0f;

        glm::vec2 avoidance_force = {0.0f, 0.0f};
        float avoidance_weight = 10.0f;

        // Pursue
        {
            glm::vec2 desired_velocity = glm::normalize(lion.locked_giraffe->mob.position - lion.mob.position) * lion.mob.max_speed;
            pursue_force = desired_velocity - lion.mob.velocity;
            pursue_force *= pursue_weight;
        }

        // Avoidance
        {
            avoidance_force = avoidance_behavior(lion.mob, engine, game);
            avoidance_force *= avoidance_weight;
        }

        lion.mob.steering_direction = truncate(pursue_force + avoidance_force, lion.mob.max_force);

        if (glm::length(lion.locked_giraffe->mob.position - lion.mob.position) <= lion.mob.radius) {
            lion.locked_giraffe->dead = true;
            engine::color_sprite(*game.sprites, lion.locked_giraffe->sprite_id, engine::color::pico8::light_gray);

            lion.locked_giraffe = nullptr;
            lion.energy = 0.0f;
            lion.mob.steering_direction = {0.0f, 0.0f};

        } else {
            lion.energy -= dt;
            if (lion.energy <= 0.0f) {
                lion.locked_giraffe = nullptr;
                lion.energy = 0.0f;
                lion.mob.steering_direction = {0.0f, 0.0f};
            }
        }
    }

    update_mob(lion.mob, game, dt);

    const engine::AtlasFrame *lion_frame = engine::atlas_frame(*game.sprites->atlas, "lion");
    assert(lion_frame);

    glm::mat4 transform = glm::mat4(1.0f);
    bool flip = lion.locked_giraffe && lion.locked_giraffe->mob.position.x < lion.mob.position.x;
    float x_offset = lion_frame->rect.size.x * lion_frame->pivot.x;
    if (flip) {
        x_offset *= -1.0f;
    }
    transform = glm::translate(transform, glm::vec3(
                                              floorf(lion.mob.position.x - x_offset),
                                              floorf(lion.mob.position.y - lion_frame->rect.size.y * (1.0f - lion_frame->pivot.y)),
                                              LION_Z_LAYER));
    transform = glm::scale(transform, {(flip ? -1.0f : 1.0f) * lion_frame->rect.size.x, lion_frame->rect.size.y, 1.0f});
    engine::transform_sprite(*game.sprites, lion.sprite_id, Matrix4f(glm::value_ptr(transform)));
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    for (Giraffe *giraffe = array::begin(game.game_state.giraffes); giraffe != array::end(game.game_state.giraffes); ++giraffe) {
        update_giraffe(*giraffe, engine, game, dt);
    }

    update_lion(game.game_state.lion, engine, game, t, dt);

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
    TempAllocator128 ta;

    if (game.game_state.debug_draw) {
        const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
        const engine::AtlasFrame *lion_frame = engine::atlas_frame(*game.sprites->atlas, "lion");
        assert(giraffe_frame);
        assert(lion_frame);

        auto debug_draw_mob = [&draw_list, &engine, &game, &ta](Mob &mob) {
            glm::vec2 origin = {mob.position.x, engine.window_rect.size.y - mob.position.y};

            Buffer ss(ta);

            // steer
            {
                glm::vec2 steer = truncate(mob.steering_direction, mob.max_force);
                steer.y *= -1;
                float steer_ratio = glm::length(steer) / mob.max_force;
                glm::vec2 end = origin + glm::normalize(steer) * (steer_ratio * 100.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(255, 0, 0, 255));
                string_stream::printf(ss, "steer: %.0f", glm::length(steer));
                draw_list->AddText(ImVec2(origin.x, origin.y + 8), IM_COL32(255, 0, 0, 255), string_stream::c_str(ss));
            }

            // velocity
            {
                glm::vec2 vel = truncate(mob.velocity, mob.max_speed);
                vel.y *= -1;
                float vel_ratio = glm::length(vel) / mob.max_speed;
                glm::vec2 end = origin + glm::normalize(vel) * (vel_ratio * 100.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(0, 255, 0, 255));
                array::clear(ss);
                string_stream::printf(ss, "veloc: %.0f", glm::length(vel));
                draw_list->AddText(ImVec2(origin.x, origin.y + 20), IM_COL32(0, 255, 0, 255), string_stream::c_str(ss));
            }

            // avoidance
            if (game.game_state.debug_avoidance) {
                glm::vec2 forward = glm::normalize(mob.steering_target - mob.position);

                forward.y *= -1;
                glm::vec2 look_ahead = origin + forward * 200.0f;

                glm::vec2 right_vector = {-forward.y, forward.x};
                glm::vec2 left_vector = {forward.y, -forward.x};

                glm::vec2 left_start = origin + left_vector * mob.radius;
                glm::vec2 left_end = look_ahead + left_vector * mob.radius;

                glm::vec2 right_start = origin + right_vector * mob.radius;
                glm::vec2 right_end = look_ahead + right_vector * mob.radius;

                draw_list->AddLine(ImVec2(left_start.x, left_start.y), ImVec2(left_end.x, left_end.y), IM_COL32(255, 255, 0, 255));
                draw_list->AddLine(ImVec2(right_start.x, right_start.y), ImVec2(right_end.x, right_end.y), IM_COL32(255, 255, 0, 255));
            }

            // radius
            {
                draw_list->AddCircle(ImVec2(origin.x, origin.y), mob.radius, IM_COL32(255, 255, 0, 255));
            }
        };

        for (Giraffe *giraffe = array::begin(game.game_state.giraffes); giraffe != array::end(game.game_state.giraffes); ++giraffe) {
            if (!giraffe->dead) {
                debug_draw_mob(giraffe->mob);
            }
        }

        // lion
        {
            debug_draw_mob(game.game_state.lion.mob);

            glm::vec2 origin = {game.game_state.lion.mob.position.x, engine.window_rect.size.y - game.game_state.lion.mob.position.y};

            Buffer ss(ta);

            // energy
            {
                string_stream::printf(ss, "energy: %.0f", glm::length(game.game_state.lion.energy));
                draw_list->AddText(ImVec2(origin.x, origin.y + 32), IM_COL32(255, 0, 0, 255), string_stream::c_str(ss));
            }
        }

        // food
        draw_list->AddCircleFilled(ImVec2(game.game_state.food.position.x, engine.window_rect.size.y - game.game_state.food.position.y), 3, IM_COL32(255, 255, 0, 255));
        draw_list->AddText(ImVec2(game.game_state.food.position.x, engine.window_rect.size.y - game.game_state.food.position.y + 8), IM_COL32(255, 255, 0, 255), "food");
    }

    // obstacles
    for (Obstacle *obstacle = array::begin(game.game_state.obstacles); obstacle != array::end(game.game_state.obstacles); ++obstacle) {
        ImU32 obstacle_color = IM_COL32(obstacle->color.r * 255.0f, obstacle->color.g * 255.0f, obstacle->color.b * 255.0f, 255);
        ImVec2 position = ImVec2(obstacle->position.x, engine.window_rect.size.y - obstacle->position.y);
        draw_list->AddCircle(position, obstacle->radius, obstacle_color, 0, 2.0f);
    }

    // debug text
    {
        using namespace string_stream;

        Buffer ss(ta);
        printf(ss, "KEY_F1: Debug Draw %s\n", game.game_state.debug_draw ? "on" : "off");
        printf(ss, "KEY_F2: Debug Avoidance %s\n", game.game_state.debug_avoidance ? "on" : "off");
        printf(ss, "KEY_1: Spawn 1 giraffe\n");
        printf(ss, "KEY_5: Spawn 5 giraffes\n");
        printf(ss, "KEY_0: Spawn 10 giraffes\n");
        printf(ss, "MOUSE_LEFT: Move food\n");
        printf(ss, "Giraffes: %u\n", array::size(game.game_state.giraffes));

        draw_list->AddText(ImVec2(8, 8), IM_COL32_WHITE, c_str(ss));
    }
}

} // namespace game
