#if defined(HAS_LUA) || defined(HAS_LUAJIT)

#include "if_game.h"

extern "C" {
#if defined(HAS_LUAJIT)
#include <luajit.h>
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "engine/math.inl"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace lua {

int push_vector2f(lua_State *L, math::Vector2f &vec) {
    lua_newtable(L);
    lua_pushnumber(L, vec.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, vec.y);
    lua_setfield(L, -2, "y");
    return 1;
}

int push_vector2(lua_State *L, math::Vector2 &vec) {
    lua_newtable(L);
    lua_pushinteger(L, vec.x);
    lua_setfield(L, -2, "x");
    lua_pushinteger(L, vec.y);
    lua_setfield(L, -2, "y");
    return 1;
}

int push_rect(lua_State *L, math::Rect &rect) {
    lua_newtable(L);
    push_vector2(L, rect.origin);
    lua_setfield(L, -2, "origin");
    push_vector2(L, rect.size);
    lua_setfield(L, -2, "size");
    luaL_setmetatable(L, RECT_METATABLE);
    return 1;
}

int push_color4f(lua_State *L, math::Color4f color) {
    math::Color4f *udata = static_cast<math::Color4f *>(lua_newuserdata(L, sizeof(math::Color4f)));
    new (udata) math::Color4f(color);
    
    luaL_getmetatable(L, COLOR4F_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

math::Color4f get_color4f(lua_State *L, int index) {
    math::Color4f *color4f = static_cast<math::Color4f*>(luaL_checkudata(L, index, COLOR4F_METATABLE));
    return *color4f;
}

math::Matrix4f get_matrix4f(lua_State *L, int index) {
    math::Matrix4f *matrix4f = static_cast<math::Matrix4f*>(luaL_checkudata(L, index, MATRIX4F_METATABLE));
    return *matrix4f;
}

int matrix4f_from_transform(lua_State* L) {
    glm::mat4* matrix = static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    math::Matrix4f *matrix_results = static_cast<math::Matrix4f *>(lua_newuserdata(L, sizeof(math::Matrix4f)));
    new (matrix_results) math::Matrix4f(glm::value_ptr(*matrix));
    luaL_getmetatable(L, MATRIX4F_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

void init_math_module(lua_State *L) {
    sol::state_view lua(L);
    sol::table math = lua["Math"].get_or_create<sol::table>();

    math.new_usertype<math::Vector2>("Vector2",
        "x", &math::Vector2::x,
        "y", &math::Vector2::y
    );

    math.new_usertype<math::Vector2f>("Vector2f",
        "x", &math::Vector2f::x,
        "y", &math::Vector2f::y
    );

    math.new_usertype<math::Rect>("Rect",
        "origin", &math::Rect::origin,
        "size", &math::Rect::size
    );

    math.new_usertype<math::Color4f>("Color4f",
        sol::initializers([](math::Color4f& obj, float r, float g, float b, float a) {
            obj.r = r;
            obj.g = g;
            obj.b = b;
            obj.a = a;
        }),
        "r", &math::Color4f::r,
        "g", &math::Color4f::g,
        "b", &math::Color4f::b,
        "a", &math::Color4f::a
    );

    math.new_usertype<math::Matrix4f>("Matrix4f",
        sol::constructors<math::Matrix4f(), math::Matrix4f(const float *)>(),
        "m", &math::Matrix4f::m,
        "identity", &math::Matrix4f::identity
    );


    lua_getglobal(L, "Math");
    lua_pushcfunction(L, matrix4f_from_transform);
    lua_setfield(L, -2, "matrix4f_from_transform");
    lua_pop(L, 1);
}
    
} // namespace lua

#endif
