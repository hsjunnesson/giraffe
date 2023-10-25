#pragma once

// Script interfaces

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

typedef struct lua_State lua_State;

namespace lua {

static const char *VEC2_METATABLE = "Glm.vec2";
static const char *VEC3_METATABLE = "Glm.vec3";
static const char *MAT4_METATABLE = "Glm.mat4";
static const char *MATRIX4F_METATABLE = "Math.Matrix4f";

void init_glm_module(lua_State *L);
void init_math_module(lua_State *L);

void initialize();

} // namespace game

#endif
