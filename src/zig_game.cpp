#if defined(HAS_ZIG)

#include "zig_game.h"
#include "zig_includes.h"

#include "game.h"
#include "memory.h"
#include "util.h"

#include <engine/engine.h>
#include <engine/log.h>
#include <engine/sprites.h>
#include <engine/atlas.h>
#include <engine/color.inl>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

extern "C" {
void script_enter(engine::Engine *engine, game::Game *game);
void script_leave(engine::Engine *engine, game::Game *game);
void script_on_input();
void script_update(engine::Engine *engine, game::Game *game, float t, float dt);
void script_render(engine::Engine *engine, game::Game *game);
void script_render_imgui(engine::Engine *engine, game::Game *game);

void fatal(const char *text) {
    log_fatal(text);
}

Rect glfw_window_rect(const void *engine) {
    Rect r;
    memcpy(&r, &((engine::Engine *)engine)->window_rect, sizeof(math::Rect));
    return r;
}

void *get_sprites(const void *game) {
    return ((game::Game *)game)->sprites;
}
 
void *get_atlas(const void *game) {
    return ((game::Game *)game)->sprites->atlas;
}

Sprite Engine_add_sprite(void *sprites, const char *sprite_name, const Color4f color) {
    engine::Sprites *p_sprites = (engine::Sprites *)sprites;
    math::Color4f c;
    memcpy(&c, &color, sizeof(Color4f));
    engine::Sprite sprite = engine::add_sprite(*p_sprites, sprite_name, c);
    Sprite s;
    memcpy(&s, &sprite, sizeof(Sprite));
    return s;
}

void Engine_update_sprites(void *sprites, float t, float dt) {
    engine::update_sprites(*(engine::Sprites *)sprites, t, dt);
}

void Engine_commit_sprites(void *sprites) {
    engine::commit_sprites(*(engine::Sprites *)sprites);
}

void Engine_render_sprites(const void *engine, const void *sprites) {
    engine::render_sprites(*(engine::Engine *)engine, *(engine::Sprites *)sprites);
}

void Engine_transform_sprite(const void *sprites, uint64_t sprite_id, const struct Matrix4f transform) {
    math::Matrix4f m;
    memcpy(&m, &transform, sizeof(Matrix4f));
    
    engine::transform_sprite(*(engine::Sprites *)sprites, sprite_id, m);
}

const AtlasFrame *Engine_atlas_frame(void *atlas, const char *sprite_name) {
    const engine::AtlasFrame *fr = engine::atlas_frame(*(engine::Atlas *)atlas, sprite_name);
    const AtlasFrame *r = (const AtlasFrame *)fr;
    return r;
}

Matrix4f Matrix4f_Identity() {
    glm::mat4 identity = glm::mat4(1.0f);
    math::Matrix4f matrix = math::Matrix4f(glm::value_ptr(identity));
    Matrix4f r;
    memcpy(&r, &matrix, sizeof(Matrix4f));
    return r;
}

Matrix4f translate(const Matrix4f m, const Vector3f v) {
    glm::mat4 transform = glm::make_mat4(m.m);
    transform = glm::translate(transform, glm::vec3(v.x, v.y, v.z));
    Matrix4f r;
    memcpy(&r, &transform, sizeof(Matrix4f));
    return r;
}

Matrix4f scale(const Matrix4f m, const Vector3f v) {
    glm::mat4 transform = glm::make_mat4(m.m);
    transform = glm::scale(transform, glm::vec3(v.x, v.y, v.z));
    Matrix4f r;
    memcpy(&r, &transform, sizeof(Matrix4f));
    return r;
}

Vector2f add_vec2(const Vector2f v1, const Vector2f v2) {
    glm::vec2 glmvec = glm::vec2(v1.x, v1.y) + glm::vec2(v2.x, v2.y);
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

Vector2f subtract_vec2(const Vector2f v1, const Vector2f v2) {
    glm::vec2 glmvec = glm::vec2(v1.x, v1.y) - glm::vec2(v2.x, v2.y);
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

Vector2f multiply_vec2_factor(const Vector2f v, float f) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y) * f;
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

Vector2f divide_vec2(const Vector2f v, float f) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y) / f;
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

Vector2f truncate_vec2(const Vector2f v, float max_length) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y);
    float length = glm::length(glmvec);
    if (length > max_length && length > 0) {
        glmvec = glmvec / length * max_length;
    }
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

Vector2f normalize_vec2(const Vector2f v) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y);
    glmvec = glm::normalize(glmvec);
    Vector2f vo;
    memcpy(&vo, &glmvec, sizeof(Vector2f));
    return vo;
}

float length_vec2(const Vector2f v) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y);
    return glm::length(glmvec);
}

float length2_vec2(const Vector2f v) {
    glm::vec2 glmvec = glm::vec2(v.x, v.y);
    return glm::length2(glmvec);
}

bool ray_circle_intersection_vec2(const Vector2f ray_origin, const Vector2f ray_direction, const Vector2f circle_center, float circle_radius, Vector2f *intersection) {
    glm::vec2 int_out;
    bool success = ray_circle_intersection(glm::vec2(ray_origin.x, ray_origin.y), glm::vec2(ray_direction.x, ray_direction.y), glm::vec2(circle_center.x, circle_center.y), circle_radius, int_out);
    intersection->x = int_out.x;
    intersection->y = int_out.y;
    return success;
}

Color4f orange;
Color4f blue;
Color4f light_gray;
Color4f green;
Color4f yellow;

} // extern "C"

namespace zig {

void initialize(foundation::Allocator &allocator) {
    log_info("Initializing Zig");
    
    memcpy(&orange, &engine::color::pico8::orange, sizeof(Color4f));
    memcpy(&blue, &engine::color::pico8::blue, sizeof(Color4f));
    memcpy(&light_gray, &engine::color::pico8::light_gray, sizeof(Color4f));
    memcpy(&green, &engine::color::pico8::green, sizeof(Color4f));
    memcpy(&yellow, &engine::color::pico8::yellow, sizeof(Color4f));
}

void close() {
}

} // namespace zig

namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    script_enter(&engine, &game);
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    script_leave(&engine, &game);
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    script_update(&engine, &game, t, dt);
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    script_render(&engine, &game);
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    script_render_imgui(&engine, &game);
}

} // namespace game

#endif
