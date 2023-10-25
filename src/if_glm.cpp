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
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include "util.h"

namespace lua {

// Constructors for glm::vec2
static int glm_vec2_new(lua_State* L) {
    int arg_count = lua_gettop(L);
    if (arg_count == 2) {
        float x = luaL_checknumber(L, 1);
        float y = luaL_checknumber(L, 2);
        glm::vec2* vec = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        new (vec) glm::vec2(x, y);
    } else {
        glm::vec2* vec = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        new (vec) glm::vec2();
    }

    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int glm_vec2_tostring(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "vec2(%f, %f)", vec->x, vec->y);
    lua_pushstring(L, buffer);
    return 1;
}

static int glm_vec2_getx(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    lua_pushnumber(L, vec->x);
    return 1;
}

static int glm_vec2_gety(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    lua_pushnumber(L, vec->y);
    return 1;
}

// Index function for vec2
static int glm_vec2_index(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "x") == 0) {
        return glm_vec2_getx(L);
    } else if (strcmp(key, "y") == 0) {
        return glm_vec2_gety(L);
    }
    return 0;  // Key doesn't exist
}

static int glm_vec2_add(lua_State* L) {
    glm::vec2* vec1 = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
    
    glm::vec2 result = *vec1 + *vec2;

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int glm_vec2_sub(lua_State* L) {
    glm::vec2* vec1 = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));

    glm::vec2 result = *vec1 - *vec2;

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int glm_vec2_mul(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    
    if (lua_isnumber(L, 2)) {
        float scalar = luaL_checknumber(L, 2);
        glm::vec2 result = *vec * scalar;

        glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *vec_res = result;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
    } else {
        glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
        glm::vec2 result = *vec * *vec2;

        glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *vec_res = result;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
    }

    return 1;
}

static int glm_vec2_div(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float scalar = luaL_checknumber(L, 2);

    glm::vec2 result = *vec / scalar;

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// Lua function to create a new vec3
static int glm_vec3_new(lua_State *L) {
    int arg_count = lua_gettop(L);
    if (arg_count == 3) {
        float x = luaL_checknumber(L, 1);
        float y = luaL_checknumber(L, 2);
        float z = luaL_checknumber(L, 3);
        
        glm::vec3* vec = static_cast<glm::vec3*>(lua_newuserdata(L, sizeof(glm::vec3)));
        new (vec) glm::vec3(x, y, z);
    } else {
        glm::vec3* vec = static_cast<glm::vec3*>(lua_newuserdata(L, sizeof(glm::vec3)));
        new (vec) glm::vec3();
    }

    luaL_getmetatable(L, VEC3_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

// Getter functions for vec3's x, y, and z components
static int glm_vec3_getx(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->x);
    return 1;
}

static int glm_vec3_gety(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->y);
    return 1;
}

static int glm_vec3_getz(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->z);
    return 1;
}

// Index function for vec3
static int glm_vec3_index(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "x") == 0) {
        return glm_vec3_getx(L);
    } else if (strcmp(key, "y") == 0) {
        return glm_vec3_gety(L);
    } else if (strcmp(key, "z") == 0) {
        return glm_vec3_getz(L);
    }
    return 0;  // Key doesn't exist
}

// Convert vec3 to string
static int glm_vec3_tostring(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    
    char buffer[100];  // Ensure this is sufficiently large
    snprintf(buffer, sizeof(buffer), "vec3(%f, %f, %f)", vec->x, vec->y, vec->z);

    lua_pushstring(L, buffer);
    return 1;
}

// Constructors for glm::mat4
static int glm_mat4_new(lua_State* L) {
    int arg_count = lua_gettop(L);
    if (arg_count == 1) {
        float f = luaL_checknumber(L, 1);
        glm::mat4* mat4 = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
        new (mat4) glm::mat4(f);
    } else {
        glm::mat4* mat4 = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
        new (mat4) glm::mat4();
    }

    luaL_getmetatable(L, MAT4_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

static int glm_mat4_tostring(lua_State* L) {
    glm::mat4* mat4= static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "mat4(...)");
    lua_pushstring(L, buffer);
    return 1;
}

static int glm_ray_circle_intersection(lua_State* L) {
    glm::vec2* ray_origin = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* ray_direction = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
    glm::vec2* circle_center = static_cast<glm::vec2*>(luaL_checkudata(L, 3, VEC2_METATABLE));
    float circle_radius = luaL_checknumber(L, 4);

    glm::vec2 intersection;
    bool result = ray_circle_intersection(*ray_origin, *ray_direction, *circle_center, circle_radius, intersection);

    lua_pushboolean(L, result);
    if (result) {
        glm::vec2* lua_intersection = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *lua_intersection = intersection;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
        return 2;  // Two return values: boolean and glm::vec2
    }

    return 1;  // Just one return value: boolean
}

static int glm_ray_line_intersection(lua_State* L) {
    glm::vec2* ray_origin = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* ray_direction = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
    glm::vec2* p1 = static_cast<glm::vec2*>(luaL_checkudata(L, 3, VEC2_METATABLE));
    glm::vec2* p2 = static_cast<glm::vec2*>(luaL_checkudata(L, 4, VEC2_METATABLE));

    glm::vec2 intersection;
    bool result = ray_line_intersection(*ray_origin, *ray_direction, *p1, *p2, intersection);

    lua_pushboolean(L, result);
    if (result) {
        glm::vec2* lua_intersection = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *lua_intersection = intersection;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
        return 2;  // Two return values: boolean and glm::vec2
    }

    return 1;  // Just one return value: boolean
}

static int glm_truncate(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float max_length = luaL_checknumber(L, 2);

    glm::vec2 result = truncate(*vector, max_length);
    
    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int glm_normalize(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2 result = glm::normalize(*vector);

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int glm_length(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float result = glm::length(*vector);
    lua_pushnumber(L, result);
    return 1;
}

static int glm_length2(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float result = glm::length2(*vector);
    lua_pushnumber(L, result);
    return 1;
}

static int glm_translate(lua_State *L) {
    glm::mat4* matrix = static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    glm::vec3* vector = static_cast<glm::vec3*>(luaL_checkudata(L, 2, VEC3_METATABLE));
    
    glm::mat4 results = glm::translate(*matrix, *vector);
    glm::mat4* mat_res = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
    *mat_res = results;

    luaL_getmetatable(L, MAT4_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

static int glm_scale(lua_State *L) {
    glm::mat4* matrix = static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    glm::vec3* vector = static_cast<glm::vec3*>(luaL_checkudata(L, 2, VEC3_METATABLE));
    
    glm::mat4 results = glm::scale(*matrix, *vector);
    glm::mat4* mat_res = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
    *mat_res = results;

    luaL_getmetatable(L, MAT4_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

void init_glm_module(lua_State *L) {
    // glm::vec2
    {
        // Register the vec2 metatable
        luaL_newmetatable(L, VEC2_METATABLE);

        // Set the __index metamethod
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, glm_vec2_index);
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunction(L, glm_vec2_tostring);
        lua_settable(L, -3);

        lua_pushstring(L, "__add");
        lua_pushcfunction(L, glm_vec2_add);
        lua_settable(L, -3);

        lua_pushstring(L, "__sub");
        lua_pushcfunction(L, glm_vec2_sub);
        lua_settable(L, -3);

        lua_pushstring(L, "__mul");
        lua_pushcfunction(L, glm_vec2_mul);
        lua_settable(L, -3);

        lua_pushstring(L, "__div");
        lua_pushcfunction(L, glm_vec2_div);
        lua_settable(L, -3);

        lua_pop(L, 1);  // Pop the metatable off the stack
    }
    
    // glm::vec3
    {
        // Register the vec3 metatable
        luaL_newmetatable(L, VEC3_METATABLE);
    
        // Set the __index metamethod
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, glm_vec3_index);
        lua_settable(L, -3);
    
        // __tostring for vec3
        lua_pushstring(L, "__tostring");
        lua_pushcfunction(L, glm_vec3_tostring);
        lua_settable(L, -3);
    
        lua_pop(L, 1);  // Pop the metatable off the stack
    }
    
    // glm::mat4
    {
        // Register the mat metatable
        luaL_newmetatable(L, MAT4_METATABLE);
    
        // __tostring for mat4
        lua_pushstring(L, "__tostring");
        lua_pushcfunction(L, glm_mat4_tostring);
        lua_settable(L, -3);
    
        lua_pop(L, 1);  // Pop the metatable off the stack
    }

    // Create a table for 'Glm'
    lua_getglobal(L, "Glm");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    lua_pushstring(L, "vec2");
    lua_pushcfunction(L, glm_vec2_new);
    lua_settable(L, -3);

    lua_pushstring(L, "vec3");
    lua_pushcfunction(L, glm_vec3_new);
    lua_settable(L, -3);

    lua_pushstring(L, "mat4");
    lua_pushcfunction(L, glm_mat4_new);
    lua_settable(L, -3);
    
    lua_pushcfunction(L, glm_ray_circle_intersection);
    lua_setfield(L, -2, "ray_circle_intersection");

    lua_pushcfunction(L, glm_ray_line_intersection);
    lua_setfield(L, -2, "ray_line_intersection");

    lua_pushcfunction(L, glm_truncate);
    lua_setfield(L, -2, "truncate");

    lua_pushcfunction(L, glm_normalize);
    lua_setfield(L, -2, "normalize");

    lua_pushcfunction(L, glm_length);
    lua_setfield(L, -2, "length");

    lua_pushcfunction(L, glm_length2);
    lua_setfield(L, -2, "length2");

    lua_pushcfunction(L, glm_translate);
    lua_setfield(L, -2, "translate");

    lua_pushcfunction(L, glm_scale);
    lua_setfield(L, -2, "scale");

   
    lua_setglobal(L, "Glm");
}

} // namespace lua

#endif
