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
#include <limits.h>

namespace {
rnd_pcg_t RANDOM_DEVICE;
const int32_t MOB_Z_LAYER = -1;
const int32_t FOOD_Z_LAYER = -2;
} // namespace

namespace game {
using namespace math;
using namespace foundation;

void spawn_giraffes(engine::Engine &engine, Game &game, int num_giraffes) {
    // Spawn giraffes outside obstacles
    const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
    assert(giraffe_frame);

    for (int i = 0; i < num_giraffes; ++i) {
        Giraffe giraffe;
        giraffe.mob.mass = 50.0f;
        giraffe.mob.max_force = 1000.0f;
        giraffe.mob.max_speed = 300.0f;
        giraffe.mob.radius = 20.0;
        giraffe.mob.position = {
            50.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.x - 100.0f),
            50.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.y - 100.0f)};

        glm::vec2 pos = giraffe.mob.position + glm::vec2(
                                                giraffe_frame->rect.size.x * giraffe_frame->pivot.x,
                                                giraffe_frame->rect.size.y * (1.0f - giraffe_frame->pivot.y));

        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "giraffe", engine::color::pico8::orange);
        giraffe.sprite_id = sprite.id;
        array::push_back(game.giraffes, giraffe);
    }
}

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;

    time_t seconds;
    time(&seconds);
    rnd_pcg_seed(&RANDOM_DEVICE, (RND_U32)seconds);

    // Spawn lake
    Obstacle obstacle;
    obstacle.position = {
        (engine.window_rect.size.x / 2) + 200.0f * rnd_pcg_nextf(&RANDOM_DEVICE) - 100.0f,
        (engine.window_rect.size.y / 2) + 200.0f * rnd_pcg_nextf(&RANDOM_DEVICE) - 100.0f
    };
    obstacle.color = engine::color::pico8::blue;
    obstacle.radius = 100.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * 100.0f;

    array::push_back(game.obstacles, obstacle);

    // Spawn trees
    for (int i = 0; i < 10; ++i) {
        Obstacle obstacle;
        obstacle.position = {
            10.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.x - 20.0f),
            10.0f + rnd_pcg_nextf(&RANDOM_DEVICE) * (engine.window_rect.size.y - 20.0f)
        };
        obstacle.color = engine::color::pico8::white;
        obstacle.radius = 20.0f;

        array::push_back(game.obstacles, obstacle);
    }

    // Spawn giraffes outside obstacles
    spawn_giraffes(engine, game, 1);

    // Spawn food
    {
        const engine::Sprite sprite = engine::add_sprite(*game.sprites, "food", engine::color::pico8::green);
        game.food.sprite_id = sprite.id;

        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(
                                                    floorf(game.food.position.x - sprite.atlas_frame->rect.size.x * sprite.atlas_frame->pivot.x),
                                                    floorf(game.food.position.y - sprite.atlas_frame->rect.size.y * (1.0f - sprite.atlas_frame->pivot.y)),
                                                    FOOD_Z_LAYER));
        transform = glm::scale(transform, {sprite.atlas_frame->rect.size.x, sprite.atlas_frame->rect.size.y, 1.0f});
        engine::transform_sprite(*game.sprites, game.food.sprite_id, Matrix4f(glm::value_ptr(transform)));
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
                game.debug_draw = !game.debug_draw;
            }
            break;
        }
        case ActionHash::DEBUG_AVOIDANCE: {
            if (pressed) {
                game.debug_avoidance = !game.debug_avoidance;
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
            game.food.position = {input_command.mouse_state.mouse_position.x, engine.window_rect.size.y - input_command.mouse_state.mouse_position.y};

            const engine::Sprite *sprite = engine::get_sprite(*game.sprites, game.food.sprite_id);
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(
                                                        floorf(game.food.position.x - sprite->atlas_frame->rect.size.x * sprite->atlas_frame->pivot.x),
                                                        floorf(game.food.position.y - sprite->atlas_frame->rect.size.y * (1.0f - sprite->atlas_frame->pivot.y)),
                                                        FOOD_Z_LAYER));
            transform = glm::scale(transform, {sprite->atlas_frame->rect.size.x, sprite->atlas_frame->rect.size.y, 1.0f});
            engine::transform_sprite(*game.sprites, game.food.sprite_id, Matrix4f(glm::value_ptr(transform)));
        }
    }
}

void update_mob(Mob &mob, Game &game, float dt) {
    float drag = 1.0f;

    // lake drags you down
    for (Obstacle *obstacle = array::begin(game.obstacles); obstacle != array::end(game.obstacles); ++obstacle) {
        const float length = glm::length(obstacle->position - mob.position);
        if (length <= obstacle->radius) {
            drag = 10.0f;
            break;
        }
    }

    const glm::vec2 drag_force = -drag * mob.velocity;
    const glm::vec2 steering_force = truncate(mob.steering_direction, mob.max_force) + drag_force;
    const glm::vec2 acceleration = (steering_force / mob.mass);

    mob.velocity = truncate(mob.velocity + acceleration, mob.max_speed);
    mob.position = mob.position + mob.velocity * dt;

    if (glm::length(mob.velocity) > 0.001f) {
        mob.orientation = atan2f(-mob.velocity.x, mob.velocity.y);
    }
}

void update_giraffe(Giraffe &giraffe, engine::Engine &engine, Game &game, float dt) {
    glm::vec2 arrival_force = {0.0f, 0.0f};
    const float arrival_weight = 1.0f;

    glm::vec2 separation_force = {0.0f, 0.0f};
    const float separation_weight = 10.0f;

    glm::vec2 avoidance_force = {0.0f, 0.0f};
    const float avoidance_weight = 10.0f;

    // arrival
    {
        const glm::vec2 target_offset = game.food.position - giraffe.mob.position;
        const float distance = glm::length(target_offset);
        const float ramped_speed = giraffe.mob.max_speed * (distance / 100.0f);
        const float clipped_speed = std::min(ramped_speed, giraffe.mob.max_speed);
        const glm::vec2 desired_velocity = (clipped_speed / distance) * target_offset;
        arrival_force = desired_velocity - giraffe.mob.velocity;
        arrival_force *= arrival_weight;
    }

    // separation
    {
        for (Giraffe *other_giraffe = array::begin(game.giraffes); other_giraffe != array::end(game.giraffes); ++other_giraffe) {
            if (other_giraffe != &giraffe) {
                glm::vec2 offset = other_giraffe->mob.position - giraffe.mob.position;
                const float distance = glm::length(offset);
                const float near_distance = giraffe.mob.radius + other_giraffe->mob.radius;
                if (distance <= near_distance) {
                    offset /= distance;
                    offset *= near_distance;
                    separation_force -= offset;
                }
            }
        }

        separation_force *= separation_weight;
    }

    // avoidance
    {
        const float look_ahead_distance = 200.0f;

        const glm::vec2 origin = giraffe.mob.position;
        const float distance_to_target = glm::length(game.food.position - origin);

        const glm::vec2 forward = glm::normalize(game.food.position - origin);

        const glm::vec2 right_vector = {forward.y, -forward.x};
        const glm::vec2 left_vector = {-forward.y, forward.x};

        const glm::vec2 left_start = origin + left_vector * giraffe.mob.radius;
        const glm::vec2 right_start = origin + right_vector * giraffe.mob.radius;

        bool left_intersects = false;
        glm::vec2 left_intersection;
        float left_intersection_distance = FLT_MAX;

        bool right_intersects = false;
        glm::vec2 right_intersection;
        float right_intersection_distance = FLT_MAX;

        // check against each obstacle
        for (Obstacle *obstacle = array::begin(game.obstacles); obstacle != array::end(game.obstacles); ++obstacle) {
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

        // check against world bounds
        const glm::vec2 left1 = {0, 0};
        const glm::vec2 left2 = {0, engine.window_rect.size.y};
        const glm::vec2 top1 = {0, 0};
        const glm::vec2 top2 = {engine.window_rect.size.x, 0};
        const glm::vec2 right1 = {engine.window_rect.size.x, 0};
        const glm::vec2 right2 = {engine.window_rect.size.x, engine.window_rect.size.y};
        const glm::vec2 bottom1 = {0, engine.window_rect.size.y};
        const glm::vec2 bottom2 = {engine.window_rect.size.x, engine.window_rect.size.y};

        glm::vec2 lines[] = {left1, left2, top1, top2, right1, right2, bottom1, bottom2};

        for (int i = 0; i < 4; ++i) {
            const glm::vec2 p1 = lines[i * 2];
            const glm::vec2 p2 = lines[i * 2 + 1];

            glm::vec2 li;
            bool did_li = ray_line_intersection(left_start, forward, p1, p2, li);
            if (did_li) {
                float distance = glm::length(li - left_start);
                if (distance <= look_ahead_distance && distance < left_intersection_distance) {
                    left_intersects = true;
                    left_intersection = li;
                    left_intersection_distance = distance;
                }
            }

            glm::vec2 ri;
            bool did_ri = ray_line_intersection(right_start, forward, p1, p2, ri);
            if (did_ri) {
                float distance = glm::length(ri - right_start);
                if (distance <= look_ahead_distance && distance < right_intersection_distance) {
                    right_intersects = true;
                    right_intersection = li;
                    right_intersection_distance = distance;
                }
            }
        }

        if (right_intersects || left_intersects) {
            if (left_intersection_distance < right_intersection_distance) {
                float ratio = left_intersection_distance / look_ahead_distance;
                avoidance_force = right_vector * (1.0f - ratio) * 100.0f;
            } else {
                float ratio = right_intersection_distance / look_ahead_distance;
                avoidance_force = left_vector * (1.0f - ratio) * 100.0f;
            }

            avoidance_force *= avoidance_weight;
        }
    }

    giraffe.mob.steering_direction = truncate(arrival_force + separation_force + avoidance_force, giraffe.mob.max_force);

    update_mob(giraffe.mob, game, dt);

    const engine::AtlasFrame *giraffe_frame = engine::atlas_frame(*game.sprites->atlas, "giraffe");
    assert(giraffe_frame);

    glm::mat4 transform = glm::mat4(1.0f);
    bool flip = giraffe.mob.velocity.x <= 0.0f;
    float x_offset = giraffe_frame->rect.size.x * giraffe_frame->pivot.x;
    if (flip) {
        x_offset *= -1.0f;
    }
    transform = glm::translate(transform, glm::vec3(
                                                floorf(giraffe.mob.position.x - x_offset),
                                                floorf(giraffe.mob.position.y - giraffe_frame->rect.size.y * (1.0f - giraffe_frame->pivot.y)),
                                                MOB_Z_LAYER));
    transform = glm::scale(transform, {(flip ? -1.0f : 1.0f) * giraffe_frame->rect.size.x, giraffe_frame->rect.size.y, 1.0f});
    engine::transform_sprite(*game.sprites, giraffe.sprite_id, Matrix4f(glm::value_ptr(transform)));
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;

    for (Giraffe *giraffe = array::begin(game.giraffes); giraffe != array::end(game.giraffes); ++giraffe) {
        update_giraffe(*giraffe, engine, game, dt);
    }

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

    if (game.debug_draw) {
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
                glm::vec2 end = origin + glm::normalize(steer) * (steer_ratio * 50.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(255, 0, 0, 255));
                string_stream::printf(ss, "steer: %.0f", glm::length(steer));
                draw_list->AddText(ImVec2(origin.x, origin.y + 8), IM_COL32(255, 0, 0, 255), string_stream::c_str(ss));
            }

            // velocity
            {
                glm::vec2 vel = truncate(mob.velocity, mob.max_speed);
                vel.y *= -1;
                float vel_ratio = glm::length(vel) / mob.max_speed;
                glm::vec2 end = origin + glm::normalize(vel) * (vel_ratio * 50.0f);
                draw_list->AddLine(ImVec2(origin.x, origin.y), ImVec2(end.x, end.y), IM_COL32(0, 255, 0, 255));
                array::clear(ss);
                string_stream::printf(ss, "veloc: %.0f", glm::length(vel));
                draw_list->AddText(ImVec2(origin.x, origin.y + 20), IM_COL32(0, 255, 0, 255), string_stream::c_str(ss));
            }

            // avoidance
            if (game.debug_avoidance) {
                glm::vec2 forward = glm::normalize(game.food.position - mob.position);

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

        for (Giraffe *giraffe = array::begin(game.giraffes); giraffe != array::end(game.giraffes); ++giraffe) {
            debug_draw_mob(giraffe->mob);
        }

        // food
        draw_list->AddCircleFilled(ImVec2(game.food.position.x, engine.window_rect.size.y - game.food.position.y), 3, IM_COL32(255, 255, 0, 255));
        draw_list->AddText(ImVec2(game.food.position.x, engine.window_rect.size.y - game.food.position.y + 8), IM_COL32(255, 255, 0, 255), "food");
    }

    // obstacles
    for (Obstacle *obstacle = array::begin(game.obstacles); obstacle != array::end(game.obstacles); ++obstacle) {
        ImU32 obstacle_color = IM_COL32(obstacle->color.r * 255.0f, obstacle->color.g * 255.0f, obstacle->color.b * 255.0f, 255);
        ImVec2 position = ImVec2(obstacle->position.x, engine.window_rect.size.y - obstacle->position.y);
        draw_list->AddCircle(position, obstacle->radius, obstacle_color, 0, 2.0f);
    }

    // num giraffes
    {
        Buffer ss(ta);
        string_stream::printf(ss, "giraffes: %u", array::size(game.giraffes));
        draw_list->AddText(ImVec2(8, 8), IM_COL32_WHITE, string_stream::c_str(ss));
    }
}

} // namespace game
