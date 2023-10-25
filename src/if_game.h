#pragma once

// Script interfaces

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

#include <stdint.h>

struct lua_State;

namespace engine {
struct Engine;
}

namespace math {
struct Rect;
struct Vector2;
struct Vector2f;
}

namespace lua {

static const char *VEC2_METATABLE = "Glm.vec2";
static const char *VEC3_METATABLE = "Glm.vec3";
static const char *MAT4_METATABLE = "Glm.mat4";
static const char *RECT_METATABLE = "Math.Rect";
static const char *MATRIX4F_METATABLE = "Math.Matrix4f";
static const char *ENGINE_METATABLE = "Engine.Engine";
static const char *SPRITES_METATABLE = "Engine.Sprites";
static const char *SPRITE_METATABLE = "Engine.Sprite";
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

int push_engine(lua_State *L, engine::Engine &engine);
int push_vector2(lua_State *L, math::Vector2 &vec);
int push_vector2f(lua_State *L, math::Vector2f &vec);
int push_rect(lua_State *L, math::Rect &rect);
int push_identifier(lua_State *L, uint64_t id);

void init_engine_module(lua_State *L);
void init_glm_module(lua_State *L);
void init_math_module(lua_State *L);

void initialize();

} // namespace game

#endif
