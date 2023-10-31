#pragma once

// Script interfaces

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

#include <stdint.h>

struct lua_State;

namespace game {
struct Game;
}

namespace engine {
struct Engine;
struct InputCommand;
struct Sprite;
struct Sprites;
}

namespace math {
struct Rect;
struct Vector2;
struct Vector2f;
struct Matrix4f;
struct Color4f;
}

namespace lua {

static const char *VEC2_METATABLE = "Glm.vec2";
static const char *VEC3_METATABLE = "Glm.vec3";
static const char *MAT4_METATABLE = "Glm.mat4";
static const char *RECT_METATABLE = "Math.Rect";
static const char *COLOR4F_METATABLE = "Math.Color4f";
static const char *MATRIX4F_METATABLE = "Math.Matrix4f";
static const char *ENGINE_METATABLE = "Engine.Engine";
static const char *SPRITES_METATABLE = "Engine.Sprites";
static const char *SPRITE_METATABLE = "Engine.Sprite";
static const char *ATLASFRAME_METATABLE = "Engine.AtlasFrame";
static const char *INPUTCOMMAND_METATABLE = "Engine.InputCommand";
static const char *GAME_METATABLE = "Game.Game";
static const char *IDENTIFIER_METATABLE = "Identifier";

struct Identifier {
    uint64_t value;
    
    explicit Identifier(uint64_t value)
    : value(value)
    {}

    bool valid() {
        return value != 0;
    }
};

int push_game(lua_State *L, game::Game &game);
int push_input_command(lua_State *L, engine::InputCommand &input_command);
int push_engine(lua_State *L, engine::Engine &engine);
int push_vector2(lua_State *L, math::Vector2 &vec);
int push_vector2f(lua_State *L, math::Vector2f &vec);
int push_rect(lua_State *L, math::Rect &rect);
int push_sprite(lua_State *L, engine::Sprite sprite);
int push_sprites(lua_State *L, engine::Sprites *sprites);
int push_identifier(lua_State *L, uint64_t id);
int push_color4f(lua_State *L, math::Color4f color);

math::Color4f get_color4f(lua_State *L, int index);
math::Matrix4f get_matrix4f(lua_State *L, int index);
uint64_t get_identifier(lua_State *L, int index);

void init_engine_module(lua_State *L);
void init_glm_module(lua_State *L);
void init_math_module(lua_State *L);

void initialize();

} // namespace game

#endif
