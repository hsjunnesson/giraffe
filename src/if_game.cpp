#include "if_game.h"

#if defined(HAS_LUA)

extern "C" {
#if defined(HAS_LUAJIT)
#include <luajit.h>
#elif defined(HAS_LUAU)
#include <luacode.h>
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}


#include "game.h"
#include "memory.h"
#include "temp_allocator.h"
#include "string_stream.h"
#include "array.h"
#include "memory.h"
#include "hash.h"
#include "util.h"
#include "rnd.h"

#include <engine/sprites.h>
#include <engine/engine.h>
#include <engine/log.h>
#include <engine/input.h>
#include <engine/action_binds.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <engine/math.inl>
#include <engine/atlas.h>
#include <engine/color.inl>
#include <engine/file.h>

#include <tuple>
#include <array>
#include <stack>
#include <imgui.h>
#include <inttypes.h>
#include <cstdarg>

#include <iostream>

namespace {
lua_State *L = nullptr;
} // namespace

using namespace foundation;
using namespace foundation::string_stream;

#if defined(HAS_LUAU)
#define lua_pushcfunc(L, fn, debugname) lua_pushcfunction(L, (fn), debugname)
#else
#define lua_pushcfunc(L, fn, debugname) lua_pushcfunction(L, (fn))
#endif

// Custom print function that uses engine logging.
int my_print(lua_State *L) {
    using namespace string_stream;

    TempAllocator128 ta;
    Buffer ss(ta);
    
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; ++i) {
        if (i > 1) {
            ss << "\t";
        }

        const char *str = lua_tolstring(L, i, NULL);
        
        if (str) {
            ss << str;
        } else {
            if (lua_isboolean(L, i)) {
                ss << (lua_toboolean(L, 1) ? "True" : "False");
            } else if (lua_isuserdata(L, i)) {
                ss << "userdata";
            } else if (lua_iscfunction(L, i)) {
                ss << "c function";
            } else if (lua_islightuserdata(L, i)) {
                ss << "light userdata";
            } else if (lua_isnil(L, i)) {
                ss << "nil";
            } else if (lua_isnone(L, i)) {
                ss << "none";
            } else if (lua_istable(L, i)) {
                ss << "table";
            } else if (lua_isthread(L, i)) {
                ss << "thread";
            } else {
                const char *type_name = luaL_typename(L, i);
                log_fatal("Could not convert argument %s to string", type_name);
            }
        }
    }

    log_info("[LUA] %s", c_str(ss));
    lua_pop(L, nargs);

    return 0;
}

void require(lua_State *L, const char* file) {
#if defined(HAS_LUAU)
	using namespace foundation::string_stream;
	using namespace foundation::array;

	TempAllocator1024 ta;
	Buffer source(ta);
	if (!engine::file::read(source, file)) {
		log_fatal("Could not load %s", file);
	}

	size_t bytecode_size = 0;
	char *bytecode = luau_compile(begin(source), size(source), NULL, &bytecode_size);
	int result = luau_load(L, file, bytecode, bytecode_size, 0);

	free(bytecode);

	if (result != 0) {
		log_fatal("Could not load bytecode from %s: %s", file, lua_tostring(L, -1));
	}
#else
    int load_status = luaL_loadfile(L, file);
    if (load_status) {
        log_fatal("Could not load %s: %s", file, lua_tostring(L, -1));
    }
#endif
}

// =================================
//        Utility functions
// =================================

namespace lua_utilities {

static const char *IDENTIFIER_METATABLE = "Identifier";
static const char *RND_PCG_T_METATABLE = "rnd_pcg_t";
static const char *HASH_METATABLE = "Foundation.Hash";

template <typename EnumType>
inline int push_enum(lua_State *L, ...) {
    va_list args;
    va_start(args, L);

    lua_newtable(L);  // Create a new table for the enum.

    while (true) {
        const char *enum_name = va_arg(args, const char *);
        if (enum_name == nullptr) { // Sentinel value to determine end
            break;
        }
        EnumType raw_enum_value = va_arg(args, EnumType);
        
        int enum_value = static_cast<int>(raw_enum_value);

        lua_pushinteger(L, enum_value);
        lua_setfield(L, -2, enum_name);
    }

    va_end(args);
    
    return 1;
}

struct Identifier {
    uint64_t value;
    
    explicit Identifier(uint64_t value)
    : value(value)
    {}

    bool valid() {
        return value != 0;
    }
};

int push_identifier(lua_State* L, uint64_t id) {
    Identifier *identifier = static_cast<lua_utilities::Identifier*>(lua_newuserdata(L, sizeof(lua_utilities::Identifier)));
    new (identifier) lua_utilities::Identifier(id);

    luaL_getmetatable(L, IDENTIFIER_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

Identifier get_identifier(lua_State *L, int index) {
    Identifier *identifier = static_cast<Identifier *>(luaL_checkudata(L, index, IDENTIFIER_METATABLE));
    return *identifier;
}

int push_hash(lua_State *L, foundation::Hash<uint64_t> &hash) {
    foundation::Hash<uint64_t> **udata = static_cast<foundation::Hash<uint64_t> **>(lua_newuserdata(L, sizeof(foundation::Hash<uint64_t> *)));
    *udata = &hash;

    luaL_getmetatable(L, HASH_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

foundation::Hash<uint64_t> *get_hash(lua_State *L, int index) {
    foundation::Hash<uint64_t> **hash = static_cast<foundation::Hash<uint64_t> **>(luaL_checkudata(L, 1, HASH_METATABLE));
    return *hash;
}

void init_module(lua_State *L) {
    // Identifier
    {
        luaL_newmetatable(L, IDENTIFIER_METATABLE);

        lua_pushstring(L, "__index");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            lua_utilities::Identifier *identifier = static_cast<lua_utilities::Identifier*>(luaL_checkudata(L, 1, IDENTIFIER_METATABLE));

            const char *key = luaL_checkstring(L, 2);
            if (strcmp(key, "valid") == 0) {
                lua_pushcfunc(L, [](lua_State *L) -> int {
                    Identifier id = get_identifier(L, 1);
                    lua_pushboolean(L, id.valid());
                    return 1;
                    },
                    "valid");
                return 1;
            }
            
            return 0;
        }, "Identifier __index");
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            lua_utilities::Identifier *identifier = static_cast<lua_utilities::Identifier*>(luaL_checkudata(L, 1, IDENTIFIER_METATABLE));
            lua_pushfstring(L, "Identifier(%llu)", identifier->value);
            return 1;
        }, "Identifier __tostring");
        lua_settable(L, -3);
        
        lua_pushstring(L, "__eq");
        lua_pushcfunc(L, [](lua_State *L)->int {
            Identifier id1 = get_identifier(L, 1);
            Identifier id2 = get_identifier(L, 2);
            
            lua_pushboolean(L, id1.value == id2.value);
            return 1;
        }, "Identifier __eq");
        lua_settable(L, -3);
        
        lua_pop(L, 1);
    }

    // rnd.h
    {
        luaL_newmetatable(L, RND_PCG_T_METATABLE);
        lua_pop(L, 1);

        lua_pushcfunc(L, [](lua_State *L) -> int {
            rnd_pcg_t *udata = static_cast<rnd_pcg_t *>(lua_newuserdata(L, sizeof(rnd_pcg_t)));
            new (udata) rnd_pcg_t();

            luaL_getmetatable(L, RND_PCG_T_METATABLE);
            lua_setmetatable(L, -2);

            return 1;
        }, "rnd_pcg_t");
        lua_setglobal(L, "rnd_pcg_t");

        lua_pushcfunc(L, [](lua_State *L) -> int {
            rnd_pcg_t *rnd = static_cast<rnd_pcg_t *>(luaL_checkudata(L, 1, RND_PCG_T_METATABLE));
            float result = rnd_pcg_nextf(rnd);
            lua_pushnumber(L, result);
            return 1;
        }, "rnd_pcg_nextf");
        lua_setglobal(L, "rnd_pcg_nextf");

        lua_pushcfunc(L, [](lua_State *L) -> int {
            rnd_pcg_t *rnd = static_cast<rnd_pcg_t *>(luaL_checkudata(L, 1, RND_PCG_T_METATABLE));
            unsigned int seed = (unsigned int)luaL_checkinteger(L, 2);
            rnd_pcg_seed(rnd, seed);
            return 0;
        }, "rnd_pcg_seed");
        lua_setglobal(L, "rnd_pcg_seed");
    }

    // foundation::Hash
    {
        luaL_newmetatable(L, HASH_METATABLE);
        lua_pop(L, 1);

        // Create a table for 'Hash'
        lua_getglobal(L, "Hash");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_newtable(L);
        }
        
        lua_pushcfunc(L, [](lua_State *L) -> int {
            foundation::Hash<uint64_t> *hash = get_hash(L, 1);
            Identifier key = get_identifier(L, 2);
            Identifier default_value = get_identifier(L, 3);

            const uint64_t &result = foundation::hash::get(*hash, key.value, default_value.value);
            return push_identifier(L, result);
        }, "get_identifier");
        lua_setfield(L, -2, "get_identifier");

        lua_setglobal(L, "Hash");
    }
}

} // namespace utilities


// ==========================
//        glm.h
// ==========================

namespace lua_glm {

static const char *VEC2_METATABLE = "Glm.vec2";
static const char *VEC3_METATABLE = "Glm.vec3";
static const char *MAT4_METATABLE = "Glm.mat4";

// Constructors for glm::vec2
int glm_vec2_new(lua_State* L) {
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

int glm_vec2_tostring(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    lua_pushfstring(L, "vec2(%f, %f)", vec->x, vec->y);
    return 1;
}

int glm_vec2_index(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));

    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "x") == 0) {
        lua_pushnumber(L, vec->x);
        return 1;
    } else if (strcmp(key, "y") == 0) {
        lua_pushnumber(L, vec->y);
        return 1;
    }
    return 0;  // Key doesn't exist
}

int glm_vec2_newindex(lua_State* L) {
    glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    const char* key = luaL_checkstring(L, 2);
    float value = luaL_checknumber(L, 3);
    
    if (strcmp(key, "x") == 0) {
        vec->x = value;
    } else if (strcmp(key, "y") == 0) {
        vec->y = value;
    } else {
        luaL_error(L, "Invalid field '%s' for glm::vec2", key);
        return 0;
    }

    return 0; 
}

int glm_vec2_add(lua_State* L) {
    glm::vec2* vec1 = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
    
    glm::vec2 result = *vec1 + *vec2;

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int glm_vec2_sub(lua_State* L) {
    glm::vec2* vec1 = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));

    glm::vec2 result = *vec1 - *vec2;

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int glm_vec2_mul(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        float scalar = luaL_checknumber(L, 1);
        glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
        glm::vec2 result = scalar * (*vec);

        glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *vec_res = result;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
    } else if (lua_isnumber(L, 2)) {
        float scalar = luaL_checknumber(L, 2);
        glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
        glm::vec2 result = (*vec) * scalar;

        glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *vec_res = result;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
    } else {
        glm::vec2* vec = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
        glm::vec2* vec2 = static_cast<glm::vec2*>(luaL_checkudata(L, 2, VEC2_METATABLE));
        glm::vec2 result = (*vec) * (*vec2);

        glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
        *vec_res = result;
        luaL_getmetatable(L, VEC2_METATABLE);
        lua_setmetatable(L, -2);
    }

    return 1;
}

int glm_vec2_div(lua_State* L) {
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
int glm_vec3_new(lua_State *L) {
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
int glm_vec3_getx(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->x);
    return 1;
}

int glm_vec3_gety(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->y);
    return 1;
}

int glm_vec3_getz(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushnumber(L, vec->z);
    return 1;
}

// Index function for vec3
int glm_vec3_index(lua_State* L) {
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
int glm_vec3_tostring(lua_State *L) {
    glm::vec3* vec = static_cast<glm::vec3*>(luaL_checkudata(L, 1, "Glm.vec3"));
    lua_pushfstring(L, "vec3(%f, %f, %f)", vec->x, vec->y, vec->z);
    return 1;
}

// Constructors for glm::mat4
int glm_mat4_new(lua_State* L) {
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

int glm_mat4_tostring(lua_State* L) {
    glm::mat4* mat4= static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    lua_pushfstring(L, "mat4(...)");
    return 1;
}

int glm_ray_circle_intersection(lua_State* L) {
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

int glm_ray_line_intersection(lua_State* L) {
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

int glm_truncate(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float max_length = luaL_checknumber(L, 2);

    glm::vec2 result = truncate(*vector, max_length);
    
    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int glm_normalize(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    glm::vec2 result = glm::normalize(*vector);

    glm::vec2* vec_res = static_cast<glm::vec2*>(lua_newuserdata(L, sizeof(glm::vec2)));
    *vec_res = result;
    luaL_getmetatable(L, VEC2_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int glm_length(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float result = glm::length(*vector);
    lua_pushnumber(L, result);
    return 1;
}

int glm_length2(lua_State *L) {
    glm::vec2* vector = static_cast<glm::vec2*>(luaL_checkudata(L, 1, VEC2_METATABLE));
    float result = glm::length2(*vector);
    lua_pushnumber(L, result);
    return 1;
}

int glm_translate(lua_State *L) {
    glm::mat4* matrix = static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    glm::vec3* vector = static_cast<glm::vec3*>(luaL_checkudata(L, 2, VEC3_METATABLE));
    
    glm::mat4 results = glm::translate(*matrix, *vector);
    glm::mat4* mat_res = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
    *mat_res = results;

    luaL_getmetatable(L, MAT4_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int glm_scale(lua_State *L) {
    glm::mat4* matrix = static_cast<glm::mat4*>(luaL_checkudata(L, 1, MAT4_METATABLE));
    glm::vec3* vector = static_cast<glm::vec3*>(luaL_checkudata(L, 2, VEC3_METATABLE));
    
    glm::mat4 results = glm::scale(*matrix, *vector);
    glm::mat4* mat_res = static_cast<glm::mat4*>(lua_newuserdata(L, sizeof(glm::mat4)));
    *mat_res = results;

    luaL_getmetatable(L, MAT4_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

void init_module(lua_State *L) {
    // glm::vec2
    {
        // Register the vec2 metatable
        luaL_newmetatable(L, VEC2_METATABLE);

        lua_pushstring(L, "__index");
        lua_pushcfunc(L, glm_vec2_index, "glm_vec2_index");
        lua_settable(L, -3);

        lua_pushstring(L, "__newindex");
        lua_pushcfunc(L, glm_vec2_newindex, "glm_vec2_newindex");
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, glm_vec2_tostring, "glm_vec2_tostring");
        lua_settable(L, -3);

        lua_pushstring(L, "__add");
        lua_pushcfunc(L, glm_vec2_add, "glm_vec2_add");
        lua_settable(L, -3);

        lua_pushstring(L, "__sub");
        lua_pushcfunc(L, glm_vec2_sub, "glm_vec2_sub");
        lua_settable(L, -3);

        lua_pushstring(L, "__mul");
        lua_pushcfunc(L, glm_vec2_mul, "glm_vec2_mul");
        lua_settable(L, -3);

        lua_pushstring(L, "__div");
        lua_pushcfunc(L, glm_vec2_div, "glm_vec2_div");
        lua_settable(L, -3);

        lua_pop(L, 1);  // Pop the metatable off the stack
    }
    
    // glm::vec3
    {
        // Register the vec3 metatable
        luaL_newmetatable(L, VEC3_METATABLE);
    
        // Set the __index metamethod
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, glm_vec3_index, "glm_vec3_index");
        lua_settable(L, -3);
    
        // __tostring for vec3
        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, glm_vec3_tostring, "glm_vec3_tostring");
        lua_settable(L, -3);
    
        lua_pop(L, 1);  // Pop the metatable off the stack
    }
    
    // glm::mat4
    {
        // Register the mat metatable
        luaL_newmetatable(L, MAT4_METATABLE);
    
        // __tostring for mat4
        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, glm_mat4_tostring, "glm_mat4_tostring");
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
    lua_pushcfunc(L, glm_vec2_new, "glm_vec2_new");
    lua_settable(L, -3);

    lua_pushstring(L, "vec3");
    lua_pushcfunc(L, glm_vec3_new, "glm_vec3_new");
    lua_settable(L, -3);

    lua_pushstring(L, "mat4");
    lua_pushcfunc(L, glm_mat4_new, "glm_mat4_new");
    lua_settable(L, -3);
    
    lua_pushcfunc(L, glm_ray_circle_intersection, "glm_ray_circle_intersection");
    lua_setfield(L, -2, "ray_circle_intersection");

    lua_pushcfunc(L, glm_ray_line_intersection, "glm_ray_line_intersection");
    lua_setfield(L, -2, "ray_line_intersection");

    lua_pushcfunc(L, glm_truncate, "glm_truncate");
    lua_setfield(L, -2, "truncate");

    lua_pushcfunc(L, glm_normalize, "glm_normalize");
    lua_setfield(L, -2, "normalize");

    lua_pushcfunc(L, glm_length, "glm_length");
    lua_setfield(L, -2, "length");

    lua_pushcfunc(L, glm_length2, "glm_length2");
    lua_setfield(L, -2, "length2");

    lua_pushcfunc(L, glm_translate, "glm_translate");
    lua_setfield(L, -2, "translate");

    lua_pushcfunc(L, glm_scale, "glm_scale");
    lua_setfield(L, -2, "scale");

   
    lua_setglobal(L, "Glm");
}

} // namespace glm


// ==========================
//        math.h
// ==========================

namespace lua_math {

static const char *RECT_METATABLE = "Math.Rect";
static const char *COLOR4F_METATABLE = "Math.Color4f";
static const char *MATRIX4F_METATABLE = "Math.Matrix4f";

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
    math::Color4f *color4f = static_cast<math::Color4f *>(luaL_checkudata(L, index, COLOR4F_METATABLE));
    return *color4f;
}

math::Matrix4f get_matrix4f(lua_State *L, int index) {
    math::Matrix4f *matrix4f = static_cast<math::Matrix4f *>(luaL_checkudata(L, index, MATRIX4F_METATABLE));
    return *matrix4f;
}

int matrix4f_from_transform(lua_State *L) {
    glm::mat4 *matrix = static_cast<glm::mat4 *>(luaL_checkudata(L, 1, lua_glm::MAT4_METATABLE));
    math::Matrix4f *matrix_results = static_cast<math::Matrix4f *>(lua_newuserdata(L, sizeof(math::Matrix4f)));
    new (matrix_results) math::Matrix4f(glm::value_ptr(*matrix));
    
    luaL_getmetatable(L, MATRIX4F_METATABLE);
    lua_setmetatable(L, -2);
    
    return 1;
}

void init_module(lua_State *L) {
    // Create a table for 'Math'
    lua_getglobal(L, "Math");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    {
        luaL_newmetatable(L, RECT_METATABLE);
        lua_pop(L, 1);

        luaL_newmetatable(L, COLOR4F_METATABLE);

        lua_pushstring(L, "__index");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            math::Color4f color = get_color4f(L, 1);

            const char *key = luaL_checkstring(L, 2);
            if (strcmp(key, "r") == 0) {
                lua_pushnumber(L, color.r);
                return 1;
            } else if (strcmp(key, "g") == 0) {
                lua_pushnumber(L, color.g);
                return 1;
            } else if (strcmp(key, "b") == 0) {
                lua_pushnumber(L, color.b);
                return 1;
            } else if (strcmp(key, "a") == 0) {
                lua_pushnumber(L, color.a);
                return 1;
            }

            return 0;
        }, "color4f __index");
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            math::Color4f *color = static_cast<math::Color4f *>(luaL_checkudata(L, 1, COLOR4F_METATABLE));
            lua_pushfstring(L, "Color4f(%f, %f, %f, %f)", color->r, color->g, color->b, color->a);
            return 1;
        }, "color4f __tostring");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, MATRIX4F_METATABLE);
        lua_pop(L, 1);
    }

    lua_pushcfunc(L, [](lua_State *L) -> int {
        float r = luaL_checknumber(L, 1);
        float g = luaL_checknumber(L, 2);
        float b = luaL_checknumber(L, 3);
        float a = luaL_checknumber(L, 4);

        math::Color4f *color = static_cast<math::Color4f *>(lua_newuserdata(L, sizeof(math::Color4f)));
        color->r = r;
        color->b = b;
        color->g = g;
        color->a = a;

        luaL_getmetatable(L, COLOR4F_METATABLE);
        lua_setmetatable(L, -2);

        return 1;
    }, "Color4f");
    lua_setfield(L, -2, "Color4f");

    lua_pushcfunc(L, matrix4f_from_transform, "matrix4f_from_transform");
    lua_setfield(L, -2, "matrix4f_from_transform");

    lua_setglobal(L, "Math");
}

} // namespace math


// ==========================
//        engine.h
// ==========================

namespace lua_engine {

static const char *ATLAS_METATABLE = "Engine.Atlas";
static const char *ENGINE_METATABLE = "Engine.Engine";
static const char *SPRITES_METATABLE = "Engine.Sprites";
static const char *SPRITE_METATABLE = "Engine.Sprite";
static const char *ATLASFRAME_METATABLE = "Engine.AtlasFrame";
static const char *INPUT_COMMAND_METATABLE = "Engine.InputCommand";
static const char *KEY_STATE_METATABLE = "Engine.KeyState";
static const char *MOUSE_STATE_METATABLE = "Engine.MouseState";
static const char *SCROLL_STATE_METATABLE = "Engine.ScrollState";
static const char *ACTION_BINDS_METATABLE = "Engine.ActionBinds";

int push_engine(lua_State *L, engine::Engine &engine) {
    engine::Engine **udata = static_cast<engine::Engine **>(lua_newuserdata(L, sizeof(engine::Engine *)));
    *udata = &engine;

    luaL_getmetatable(L, ENGINE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_atlas_frame(lua_State *L, engine::AtlasFrame atlas_frame) {
    lua_newtable(L);
    lua_math::push_vector2f(L, atlas_frame.pivot);
    lua_setfield(L, -2, "pivot");
    lua_math::push_rect(L, atlas_frame.rect);
    lua_setfield(L, -2, "rect");
    return 1;
}

int engine_atlas_frame(lua_State *L) {
    engine::Atlas **udata = static_cast<engine::Atlas**>(luaL_checkudata(L, 1, ATLAS_METATABLE));
    engine::Atlas *atlas = *udata;

    const char *sprite_name = luaL_checkstring(L, 2);

    const engine::AtlasFrame *atlas_frame = engine::atlas_frame(*atlas, sprite_name);
    assert(atlas_frame != nullptr);

    return push_atlas_frame(L, *atlas_frame);
}

int push_sprite(lua_State *L, engine::Sprite sprite) {
    engine::Sprite *s = static_cast<engine::Sprite *>(lua_newuserdata(L, sizeof(engine::Sprite)));
    new (s)engine::Sprite(sprite);
    
    luaL_getmetatable(L, SPRITE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_sprites(lua_State *L, engine::Sprites *sprites) {
    engine::Sprites **udata = static_cast<engine::Sprites **>(lua_newuserdata(L, sizeof(engine::Sprites *)));
    *udata = sprites;

    luaL_getmetatable(L, SPRITES_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_action_binds(lua_State *L, engine::ActionBinds &action_binds) {
    engine::ActionBinds **udata = static_cast<engine::ActionBinds **>(lua_newuserdata(L, sizeof(engine::ActionBinds *)));
    *udata = &action_binds;

    luaL_getmetatable(L, ACTION_BINDS_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int engine_index(lua_State* L) {
    engine::Engine **udata = static_cast<engine::Engine**>(luaL_checkudata(L, 1, ENGINE_METATABLE));
    engine::Engine *engine = *udata;

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "window_rect") == 0) {
        return lua_math::push_rect(L, engine->window_rect);
    }

    return 0;
}

int engine_add_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    const char *sprite_name = luaL_checkstring(L, 2);
    math::Color4f color = engine::color::white;
    
    if (lua_isuserdata(L, 3)) {
        color = lua_math::get_color4f(L, 3);
    }
    
    engine::Sprite sprite = engine::add_sprite(**sprites, sprite_name, color);
    return push_sprite(L, sprite);
}

int engine_remove_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    lua_utilities::Identifier id = lua_utilities::get_identifier(L, 2);
    engine::remove_sprite(**sprites, id.value);
    return 0;
}

int engine_get_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    lua_utilities::Identifier id = lua_utilities::get_identifier(L, 2);
    const engine::Sprite *sprite = engine::get_sprite(**sprites, id.value);
    
    if (sprite) {
        push_sprite(L, *sprite);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

math::Matrix4f get_matrix4f(lua_State *L, int index) {
    math::Matrix4f matrix;

    if (lua_istable(L, index)) {
        for (int i = 0; i < 16; ++i) {
            lua_rawgeti(L, index, i + 1);
            matrix.m[i] = static_cast<float>(luaL_checknumber(L, -1));
            lua_pop(L, 1); // Pop the number, keep the matrix table
        }
    } else {
        luaL_error(L, "Expected a table for the matrix");
    }

    return matrix;
}

int engine_transform_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    lua_utilities::Identifier id = lua_utilities::get_identifier(L, 2);
    math::Matrix4f transform = get_matrix4f(L, 3);
    
    engine::transform_sprite(**sprites, id.value, transform);
    return 0;
}

int engine_color_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    lua_utilities::Identifier id = lua_utilities::get_identifier(L, 2);
    math::Color4f color = lua_math::get_color4f(L, 3);
    
    engine::color_sprite(**sprites, id.value, color);
    return 0;
}

int engine_update_sprites(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    float t = static_cast<float>(luaL_checknumber(L, 2));
    float dt = static_cast<float>(luaL_checknumber(L, 3));
    engine::update_sprites(**sprites, t, dt);
    return 0;
}

int engine_commit_sprites(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    engine::commit_sprites(**sprites);
    return 0;
}

int engine_render_sprites(lua_State *L) {
    engine::Engine **engine = static_cast<engine::Engine **>(luaL_checkudata(L, 1, ENGINE_METATABLE));
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 2, SPRITES_METATABLE));
    engine::render_sprites(**engine, **sprites);
    return 0;
}

int engine_transform_sprites(lua_State *L) {
    // Check that we have the correct number of arguments
    if (lua_gettop(L) != 4) {
        luaL_error(L, "Expected 4 arguments: sprites, t, dt, sprite_transforms");
    }

    // First argument: Sprites userdata
    engine::Sprites** sprites = static_cast<engine::Sprites**>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    
    // Second argument: time 't'
    float t = static_cast<float>(luaL_checknumber(L, 2));
    
    // Third argument: delta time 'dt'
    float dt = static_cast<float>(luaL_checknumber(L, 3));
    
    // Fourth argument: table of sprite transforms
    luaL_checktype(L, 4, LUA_TTABLE); // Make sure the fourth argument is a table
    
    // Get the length of the table using lua_objlen for LuaJIT
    int table_length = lua_objlen(L, 4);

    // Iterate over each entry in the table
    for (int i = 1; i <= table_length; ++i) {
        // Push the current index onto the stack and get the table at that index
        lua_rawgeti(L, 4, i);

        // Ensure we have a table at the current index
        if (lua_istable(L, -1)) {
            // Get sprite_id, assuming it's at index 1 of the nested table
            lua_pushinteger(L, 1);
            lua_gettable(L, -2);
            lua_utilities::Identifier sprite_id = lua_utilities::get_identifier(L, -1);
            lua_pop(L, 1); // Pop sprite_id off the stack
            
            // Get transform, assuming it's at index 2 of the nested table
            lua_pushinteger(L, 2);
            lua_gettable(L, -2);
            // Here you'd extract the matrix data similar to how you handled it in `engine_transform_sprite`
            math::Matrix4f transform = lua_engine::get_matrix4f(L, -1);
            lua_pop(L, 1); // Pop transform off the stack
            
            // Now you have sprite_id and transform and can do whatever is necessary with them
            engine::transform_sprite(**sprites, sprite_id.value, transform);
        } else {
            luaL_error(L, "Expected a table for each sprite transform");
        }

        // Clean up the stack (pop the current sprite transformation table)
        lua_pop(L, 1);
    }
    
    engine::update_sprites(**sprites, t, dt);
    engine::commit_sprites(**sprites);


    return 0; // Number of return values
}

int sprite_identifier(lua_State *L) {
    engine::Sprite *sprite = static_cast<engine::Sprite *>(luaL_checkudata(L, 1, SPRITE_METATABLE));
    return lua_utilities::push_identifier(L, sprite->id);
}

int sprite_index(lua_State *L) {
    engine::Sprite *sprite = static_cast<engine::Sprite*>(luaL_checkudata(L, 1, SPRITE_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "atlas_frame") == 0) {
        return push_atlas_frame(L, *sprite->atlas_frame);
    } else if (strcmp(key, "identifier") == 0) {
        lua_pushcfunc(L, [](lua_State *L) -> int {
            engine::Sprite *sprite = static_cast<engine::Sprite*>(luaL_checkudata(L, 1, SPRITE_METATABLE));
            return lua_utilities::push_identifier(L, sprite->id);
        }, "sprite.identifier");
        return 1;
    }
    
    return 0;
}

int sprites_index(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "atlas") == 0) {
        engine::Atlas **atlas = static_cast<engine::Atlas **>(lua_newuserdata(L, sizeof(engine::Atlas)));
        *atlas = (*sprites)->atlas;

        luaL_getmetatable(L, ATLAS_METATABLE);
        lua_setmetatable(L, -2);

        return 1;
    }
    
    return 0;
}

int atlasframe_index(lua_State *L) {
    engine::AtlasFrame *atlas_frame = static_cast<engine::AtlasFrame *>(luaL_checkudata(L, 1, ATLASFRAME_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "pivot") == 0) {
        return lua_math::push_vector2f(L, atlas_frame->pivot);
    } else if (strcmp(key, "rect") == 0) {
        return lua_math::push_rect(L, atlas_frame->rect);
    }
    
    return 0;
}

int push_key_state(lua_State *L, engine::KeyState key_state) {
    engine::KeyState *k = static_cast<engine::KeyState *>(lua_newuserdata(L, sizeof(engine::KeyState)));
    new (k) engine::KeyState(key_state);

    luaL_getmetatable(L, KEY_STATE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_mouse_state(lua_State *L, engine::MouseState mouse_state) {
    engine::MouseState *m = static_cast<engine::MouseState *>(lua_newuserdata(L, sizeof(engine::MouseState)));
    new (m) engine::MouseState(mouse_state);

    luaL_getmetatable(L, MOUSE_STATE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_scroll_state(lua_State *L, engine::ScrollState scroll_state) {
    engine::ScrollState *s = static_cast<engine::ScrollState *>(lua_newuserdata(L, sizeof(engine::ScrollState)));
    new (s) engine::ScrollState(scroll_state);

    luaL_getmetatable(L, SCROLL_STATE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_input_command(lua_State *L, engine::InputCommand input_command) {
    engine::InputCommand *ic = static_cast<engine::InputCommand *>(lua_newuserdata(L, sizeof(engine::InputCommand)));
    new (ic) engine::InputCommand(input_command);

    luaL_getmetatable(L, INPUT_COMMAND_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int key_state_index(lua_State *L) {
    engine::KeyState *key_state = static_cast<engine::KeyState*>(luaL_checkudata(L, 1, KEY_STATE_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "keycode") == 0) {
        lua_pushinteger(L, key_state->keycode);
        return 1;
    } else if (strcmp(key, "trigger_state") == 0) {
        lua_pushinteger(L, static_cast<int>(key_state->trigger_state));
        return 1;
    } else if (strcmp(key, "shift_state") == 0) {
        lua_pushboolean(L, key_state->shift_state);
        return 1;
    } else if (strcmp(key, "alt_state") == 0) {
        lua_pushboolean(L, key_state->alt_state);
        return 1;
    } else if (strcmp(key, "ctrl_state") == 0) {
        lua_pushboolean(L, key_state->ctrl_state);
        return 1;
    }

    return 0;
}

int mouse_state_index(lua_State *L) {
    engine::MouseState *mouse_state = static_cast<engine::MouseState*>(luaL_checkudata(L, 1, MOUSE_STATE_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "mouse_action") == 0) {
        lua_pushinteger(L, static_cast<int>(mouse_state->mouse_action));
        return 1;
    } else if (strcmp(key, "mouse_position") == 0) {
        lua_math::push_vector2f(L, mouse_state->mouse_position);
        return 1;
    } else if (strcmp(key, "mouse_relative_motion") == 0) {
        lua_math::push_vector2f(L, mouse_state->mouse_relative_motion);
        return 1;
    } else if (strcmp(key, "mouse_left_state") == 0) {
        lua_pushinteger(L, static_cast<int>(mouse_state->mouse_left_state));
        return 1;
    } else if (strcmp(key, "mouse_right_state") == 0) {
        lua_pushinteger(L, static_cast<int>(mouse_state->mouse_right_state));
        return 1;
    }

    return 0;
}

int scroll_state_index(lua_State *L) {
    engine::ScrollState *scroll_state = static_cast<engine::ScrollState*>(luaL_checkudata(L, 1, SCROLL_STATE_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "x_offset") == 0) {
        lua_pushnumber(L, scroll_state->x_offset);
        return 1;
    } else if (strcmp(key, "y_offset") == 0) {
        lua_pushnumber(L, scroll_state->y_offset);
        return 1;
    }

    return 0;
}

int input_command_index(lua_State *L) {
    engine::InputCommand *input_command = static_cast<engine::InputCommand*>(luaL_checkudata(L, 1, INPUT_COMMAND_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "input_type") == 0) {
        lua_pushinteger(L, static_cast<int>(input_command->input_type));
        return 1;
    } else if (strcmp(key, "key_state") == 0) {
        assert(input_command->input_type == engine::InputType::Key);
        return push_key_state(L, input_command->key_state);
    } else if (strcmp(key, "mouse_state") == 0) {
        assert(input_command->input_type == engine::InputType::Mouse);
        return push_mouse_state(L, input_command->mouse_state);
    } else if (strcmp(key, "scroll_state") == 0) {
        assert(input_command->input_type == engine::InputType::Scroll);
        return push_scroll_state(L, input_command->scroll_state);
    }

    return 0;
}

void init_module(lua_State *L) {
    // Create a table for 'Engine'
    lua_getglobal(L, "Engine");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    // engine.h
    {
        luaL_newmetatable(L, ENGINE_METATABLE);

        lua_pushstring(L, "__index");
        lua_pushcfunc(L, engine_index, "engine_index");
        lua_settable(L, -3);

        lua_pop(L, 1);
    }

    // sprites.h
    {
        luaL_newmetatable(L, SPRITE_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, sprite_index, "sprite_index");
        lua_settable(L, -3);

        lua_pushstring(L, "identifier");
        lua_pushcfunc(L, sprite_identifier, "sprite_identifier");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, SPRITES_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, sprites_index, "sprites_index");
        lua_settable(L, -3);
        
        lua_pop(L, 1);
        
        lua_pushcfunc(L, engine_add_sprite, "engine_add_sprite");
        lua_setfield(L, -2, "add_sprite");
    
        lua_pushcfunc(L, engine_remove_sprite, "engine_remove_sprite");
        lua_setfield(L, -2, "remove_sprite");
    
        lua_pushcfunc(L, engine_get_sprite, "engine_get_sprite");
        lua_setfield(L, -2, "get_sprite");
    
        lua_pushcfunc(L, engine_transform_sprite, "engine_transform_sprite");
        lua_setfield(L, -2, "transform_sprite");
    
        lua_pushcfunc(L, engine_color_sprite, "engine_color_sprite");
        lua_setfield(L, -2, "color_sprite");
    
        lua_pushcfunc(L, engine_update_sprites, "engine_update_sprites");
        lua_setfield(L, -2, "update_sprites");
    
        lua_pushcfunc(L, engine_commit_sprites, "engine_commit_sprites");
        lua_setfield(L, -2, "commit_sprites");
    
        lua_pushcfunc(L, engine_render_sprites, "engine_render_sprites");
        lua_setfield(L, -2, "render_sprites");

        lua_pushcfunc(L, engine_transform_sprites, "engine_transform_sprites");
        lua_setfield(L, -2, "transform_sprites");
    }

    // atlas.h
    {
        luaL_newmetatable(L, ATLASFRAME_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, atlasframe_index, "atlasframe_index");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, ATLAS_METATABLE);
        lua_pop(L, 1);

        lua_pushcfunc(L, engine_atlas_frame, "engine_atlas_frame");
        lua_setfield(L, -2, "atlas_frame");
    }
    
    // input.h
    {
        luaL_newmetatable(L, INPUT_COMMAND_METATABLE);
       
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, input_command_index, "input_command_index");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, KEY_STATE_METATABLE);
       
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, key_state_index, "key_state_index");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, MOUSE_STATE_METATABLE);
       
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, mouse_state_index, "mouse_state_index");
        lua_settable(L, -3);

        lua_pop(L, 1);

        luaL_newmetatable(L, SCROLL_STATE_METATABLE);
       
        lua_pushstring(L, "__index");
        lua_pushcfunc(L, scroll_state_index, "scroll_state_index");
        lua_settable(L, -3);

        lua_pop(L, 1);

        lua_utilities::push_enum<engine::InputType>(L, 
            "None", engine::InputType::None,
            "Mouse", engine::InputType::Mouse,
            "Key", engine::InputType::Key,
            "Scroll", engine::InputType::Scroll,
            nullptr);
        lua_setfield(L, -2, "InputType");
        
        lua_utilities::push_enum<engine::MouseAction>(L, 
            "None", engine::MouseAction::None,
            "MouseMoved", engine::MouseAction::MouseMoved,
            "MouseTrigger", engine::MouseAction::MouseTrigger,
            nullptr);
        lua_setfield(L, -2, "MouseAction");
        
        lua_utilities::push_enum<engine::TriggerState>(L, 
            "None", engine::TriggerState::None,
            "Pressed", engine::TriggerState::Pressed,
            "Released", engine::TriggerState::Released,
            "Repeated", engine::TriggerState::Repeated,
            nullptr);
        lua_setfield(L, -2, "TriggerState");
        
        lua_utilities::push_enum<engine::CursorMode>(L, 
            "Normal", engine::CursorMode::Normal,
            "Hidden", engine::CursorMode::Hidden,
            nullptr);
        lua_setfield(L, -2, "CursorMode");
    }
    
    // action_binds.h
    {
        luaL_newmetatable(L, ACTION_BINDS_METATABLE);

        lua_pushstring(L, "__index");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            engine::ActionBinds **action_binds = static_cast<engine::ActionBinds**>(luaL_checkudata(L, 1, ACTION_BINDS_METATABLE));

            const char *key = luaL_checkstring(L, 2);
            if (strcmp(key, "bind_actions") == 0) {
                return lua_utilities::push_hash(L, (*action_binds)->bind_actions);
            }

            return 0;
        }, "action_binds __index");
        lua_settable(L, -3);

        lua_pushstring(L, "__tostring");
        lua_pushcfunc(L, [](lua_State *L) -> int {
            lua_pushstring(L, "ActionBinds");
            return 1;
        }, "action_binds __tostring");
        lua_settable(L, -3);

        lua_pop(L, 1);


        lua_newtable(L);

        for (int i = static_cast<int>(engine::ActionBindsBind::FIRST) + 1; i < static_cast<int>(engine::ActionBindsBind::LAST); ++i) {
            engine::ActionBindsBind bind = static_cast<engine::ActionBindsBind>(i);
            lua_pushinteger(L, i);
            lua_setfield(L, -2, engine::bind_descriptor(bind));
        }

        lua_setfield(L, -2, "ActionBindsBind");


        lua_pushcfunc(L, [](lua_State *L) -> int {
            engine::ActionBindsBind bind = static_cast<engine::ActionBindsBind>(luaL_checkinteger(L, 1));
            const char *bind_descriptor = engine::bind_descriptor(bind);
            lua_pushstring(L, bind_descriptor);
            return 1;
        }, "bind_descriptor");
        lua_setfield(L, -2, "bind_descriptor");

        lua_pushcfunc(L, [](lua_State *L) -> int {
            int16_t keycode = static_cast<int16_t>(luaL_checkinteger(L, 1));
            engine::ActionBindsBind bind = engine::bind_for_keycode(keycode);
            lua_pushinteger(L, static_cast<int>(bind));
            return 1;
        }, "bind_for_keycode");
        lua_setfield(L, -2, "bind_for_keycode");

        lua_pushcfunc(L, [](lua_State *L) -> int {
            engine::InputCommand *input_command = static_cast<engine::InputCommand *>(luaL_checkudata(L, 1, INPUT_COMMAND_METATABLE));
            uint64_t result = engine::action_key_for_input_command(*input_command);
            return lua_utilities::push_identifier(L, result);
        }, "action_key_for_input_command");
        lua_setfield(L, -2, "action_key_for_input_command");
    }
    
    // color.inl
    {
        lua_newtable(L);
        
        lua_math::push_color4f(L, engine::color::black);
        lua_setfield(L, -2, "black");
        lua_math::push_color4f(L, engine::color::white);
        lua_setfield(L, -2, "white");
        lua_math::push_color4f(L, engine::color::red);
        lua_setfield(L, -2, "red");
        lua_math::push_color4f(L, engine::color::green);
        lua_setfield(L, -2, "green");
        lua_math::push_color4f(L, engine::color::blue);
        lua_setfield(L, -2, "blue");
        
        lua_newtable(L);

        lua_math::push_color4f(L, engine::color::pico8::black);
        lua_setfield(L, -2, "black");
        lua_math::push_color4f(L, engine::color::pico8::dark_blue);
        lua_setfield(L, -2, "dark_blue");
        lua_math::push_color4f(L, engine::color::pico8::dark_purple);
        lua_setfield(L, -2, "dark_purple");
        lua_math::push_color4f(L, engine::color::pico8::dark_green);
        lua_setfield(L, -2, "dark_green");
        lua_math::push_color4f(L, engine::color::pico8::brown);
        lua_setfield(L, -2, "brown");
        lua_math::push_color4f(L, engine::color::pico8::dark_gray);
        lua_setfield(L, -2, "dark_gray");
        lua_math::push_color4f(L, engine::color::pico8::light_gray);
        lua_setfield(L, -2, "light_gray");
        lua_math::push_color4f(L, engine::color::pico8::white);
        lua_setfield(L, -2, "white");
        lua_math::push_color4f(L, engine::color::pico8::red);
        lua_setfield(L, -2, "red");
        lua_math::push_color4f(L, engine::color::pico8::orange);
        lua_setfield(L, -2, "orange");
        lua_math::push_color4f(L, engine::color::pico8::yellow);
        lua_setfield(L, -2, "yellow");
        lua_math::push_color4f(L, engine::color::pico8::green);
        lua_setfield(L, -2, "green");
        lua_math::push_color4f(L, engine::color::pico8::blue);
        lua_setfield(L, -2, "blue");
        lua_math::push_color4f(L, engine::color::pico8::green);
        lua_setfield(L, -2, "green");
        lua_math::push_color4f(L, engine::color::pico8::indigo);
        lua_setfield(L, -2, "indigo");
        lua_math::push_color4f(L, engine::color::pico8::pink);
        lua_setfield(L, -2, "pink");
        lua_math::push_color4f(L, engine::color::pico8::peach);
        lua_setfield(L, -2, "peach");

        lua_setfield(L, -2, "Pico8");

        lua_setfield(L, -2, "Color");
    }

    // Pop Engine table
    lua_setglobal(L, "Engine");
}

} // namespace engine


// ==========================
//        game.h
// ==========================

namespace lua_game {

static const char *GAME_METATABLE = "Game.Game";

int push_game(lua_State *L, game::Game &game) {
    game::Game **udata = static_cast<game::Game **>(lua_newuserdata(L, sizeof(game::Game *)));
    *udata = &game;

    luaL_getmetatable(L, GAME_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int game_index(lua_State* L) {
    game::Game **udata = static_cast<game::Game**>(luaL_checkudata(L, 1, GAME_METATABLE));
    game::Game *game = *udata;

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "sprites") == 0) {
        return lua_engine::push_sprites(L, game->sprites);
    } else if (strcmp(key, "action_binds") == 0) {
        return lua_engine::push_action_binds(L, *game->action_binds);
    }

    return 0;
}

// Create the Game module and export all functions, types, and enums that's used in this game.
void init_module(lua_State *L) {
    // Create a table for 'Game'
    lua_getglobal(L, "Game");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    luaL_newmetatable(L, GAME_METATABLE);

    lua_pushstring(L, "__index");
    lua_pushcfunc(L, game_index, "game_index");
    lua_settable(L, -3);

    lua_pop(L, 1);


    lua_newtable(L);

    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::NONE));
    lua_setfield(L, -2, "NONE");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::QUIT));
    lua_setfield(L, -2, "QUIT");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::DEBUG_DRAW));
    lua_setfield(L, -2, "DEBUG_DRAW");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::DEBUG_AVOIDANCE));
    lua_setfield(L, -2, "DEBUG_AVOIDANCE");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::ADD_ONE));
    lua_setfield(L, -2, "ADD_ONE");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::ADD_FIVE));
    lua_setfield(L, -2, "ADD_FIVE");
    lua_utilities::push_identifier(L, static_cast<uint64_t>(game::ActionHash::ADD_TEN));
    lua_setfield(L, -2, "ADD_TEN");

    lua_setfield(L, -2, "ActionHash");


    lua_newtable(L);

    lua_pushinteger(L, static_cast<int>(game::AppState::None));
    lua_setfield(L, -2, "None");
    lua_pushinteger(L, static_cast<int>(game::AppState::Initializing));
    lua_setfield(L, -2, "Initializing");
    lua_pushinteger(L, static_cast<int>(game::AppState::Playing));
    lua_setfield(L, -2, "Playing");
    lua_pushinteger(L, static_cast<int>(game::AppState::Quitting));
    lua_setfield(L, -2, "Quitting");
    lua_pushinteger(L, static_cast<int>(game::AppState::Terminate));
    lua_setfield(L, -2, "Terminate");

    lua_setfield(L, -2, "AppState");

    lua_pushcfunc(L, [](lua_State *L) -> int {
        engine::Engine **engine = static_cast<engine::Engine**>(luaL_checkudata(L, 1, lua_engine::ENGINE_METATABLE));
        game::Game **game = static_cast<game::Game**>(luaL_checkudata(L, 2, GAME_METATABLE));
        game::AppState app_state = static_cast<game::AppState>(luaL_checkinteger(L, 3));

        game::transition(**engine, *game, app_state);
        return 0;
    }, "Engine.transition");
    lua_setfield(L, -2, "transition");

    // Pop Game table
    lua_setglobal(L, "Game");
}

} // namespace game


// ==========================
//        imgui.h
// ==========================

namespace lua_imgui {

static const char *DRAW_LIST_METATABLE = "Imgui.ImDrawList";
static const char *IMVEC2_METATABLE = "Imgui.ImVec2";

ImVec2 get_imvec2(lua_State *L, int index) {
    ImVec2 *vec = static_cast<ImVec2 *>(luaL_checkudata(L, index, IMVEC2_METATABLE));
    return *vec;
}

int push_imu32(lua_State *L, ImU32 color) {
    lua_pushlightuserdata(L, reinterpret_cast<void*>(static_cast<uintptr_t>(color)));
    return 1;
}

ImU32 get_imu32(lua_State *L, int index) {
    return static_cast<ImU32>(reinterpret_cast<uintptr_t>(lua_touserdata(L, index)));
}

void init_module(lua_State *L) {
    // Create a table for 'Imgui'
    lua_getglobal(L, "Imgui");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    luaL_newmetatable(L, DRAW_LIST_METATABLE);

    lua_pushstring(L, "__index");
    lua_pushcfunc(L, [](lua_State *L) -> int {
        const char *key = luaL_checkstring(L, 2);
        if (strcmp(key, "AddLine") == 0) {
            lua_pushcfunc(L, [](lua_State *L) -> int {
                ImDrawList **udata = static_cast<ImDrawList**>(luaL_checkudata(L, 1, DRAW_LIST_METATABLE));
                ImDrawList *draw_list = *udata;
                ImVec2 p1 = get_imvec2(L, 2);
                ImVec2 p2 = get_imvec2(L, 3);
                ImU32 col = get_imu32(L, 4);
                float thickness = luaL_checknumber(L, 5);
                draw_list->AddLine(p1, p2, col, thickness);
                return 0;
            }, "AddLine");
            return 1;
        } else if (strcmp(key, "AddText") == 0) {
            lua_pushcfunc(L, [](lua_State *L) -> int {
                ImDrawList **udata = static_cast<ImDrawList**>(luaL_checkudata(L, 1, DRAW_LIST_METATABLE));
                ImDrawList *draw_list = *udata;
                ImVec2 pos = get_imvec2(L, 2);
                ImU32 col = get_imu32(L, 3);
                const char *text = luaL_checkstring(L, 4);
                draw_list->AddText(pos, col, text);
                return 0;
            }, "AddText");
            return 1;
        } else if (strcmp(key, "AddCircle") == 0) {
            lua_pushcfunc(L, [](lua_State *L) -> int {
                ImDrawList **udata = static_cast<ImDrawList**>(luaL_checkudata(L, 1, DRAW_LIST_METATABLE));
                ImDrawList *draw_list = *udata;
                ImVec2 center = get_imvec2(L, 2);
                float radius = luaL_checknumber(L, 3);
                ImU32 col = get_imu32(L, 4);
                int segments = luaL_checkinteger(L, 5);
                float thickness = luaL_checknumber(L, 6);
                draw_list->AddCircle(center, radius, col, segments, thickness);
                return 0;
            }, "AddCircle");
            return 1;
        }

        return 0;
    }, "DrawList __index");
    lua_settable(L, -3);

    lua_pop(L, 1);

    luaL_newmetatable(L, IMVEC2_METATABLE);
    lua_pop(L, 1);


    lua_pushcfunc(L, [](lua_State *L) -> int {
        float x = luaL_checknumber(L, 1);
        float y = luaL_checknumber(L, 2);

        ImVec2 *udata = static_cast<ImVec2 *>(lua_newuserdata(L, sizeof(ImVec2)));
        udata->x = x;
        udata->y = y;

        luaL_getmetatable(L, IMVEC2_METATABLE);
        lua_setmetatable(L, -2);

        return 1;
    }, "ImVec2");
    lua_setfield(L, -2, "Imvec2");

    lua_pushcfunc(L, [](lua_State *L) -> int {
        ImDrawList **udata = static_cast<ImDrawList **>(lua_newuserdata(L, sizeof(ImDrawList *)));
        *udata = ImGui::GetForegroundDrawList();

        luaL_getmetatable(L, DRAW_LIST_METATABLE);
        lua_setmetatable(L, -2);

        return 1;
    }, "ImGui.GetForegroundDrawList");
    lua_setfield(L, -2, "GetForegroundDrawList");

    lua_pushcfunc(L, [](lua_State *L) -> int {
        int r = luaL_checkinteger(L, 1);
        int g = luaL_checkinteger(L, 2);
        int b = luaL_checkinteger(L, 3);
        int a = luaL_checkinteger(L, 4);
        ImU32 col = IM_COL32(r, g, b, a);
        return push_imu32(L, col);
    }, "IM_COL32");
    lua_setfield(L, -2, "IM_COL32");

    lua_setglobal(L, "Imgui");
}

} // namespace imgui

static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
    foundation::Allocator &allocator = *(static_cast<foundation::Allocator *>(ud));
    
    if (nsize == 0) {
        if (ptr) {
            allocator.deallocate(ptr);
        }
        
        return nullptr;
    }
    
    void *new_ptr = allocator.allocate(nsize);
    if (!new_ptr) {
        log_fatal("Could not allocate memory");
    }

    if (ptr) {
        memcpy(new_ptr, ptr, std::min(osize, nsize));
        allocator.deallocate(ptr);
    }
    
    return new_ptr;
}

void lua::initialize(foundation::Allocator &allocator) {
    log_info("Initializing lua");

#if defined(HAS_LUAJIT)
    L = luaL_newstate();
#else
    L = lua_newstate(l_alloc, &allocator);
#endif

    luaL_openlibs(L);

    lua_pushcfunc(L, my_print, "print");
    lua_setglobal(L, "print");

    lua_utilities::init_module(L);
    lua_glm::init_module(L);
    lua_math::init_module(L);
    lua_engine::init_module(L);
    lua_game::init_module(L);
    lua_imgui::init_module(L);

	require(L, "scripts/main.lua");

    int exec_status = lua_pcall(L, 0, 0, 0);
    if (exec_status) {
        log_fatal("Could not run scripts/main.lua: %s", lua_tostring(L, -1));
    }
}

void lua::close() {
    lua_close(L);
    L = nullptr;
}

// These are the implementation of the functions declared in game.cpp so that all the gameplay implementation runs in Lua.
namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "on_enter");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in on_enter: %s", error_msg);
    }
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "on_leave");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in on_leave: %s", error_msg);
    }
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "on_input");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    lua_engine::push_input_command(L, input_command);
    if (lua_pcall(L, 3, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in on_input: %s", error_msg);
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "update");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    lua_pushnumber(L, t);
    lua_pushnumber(L, dt);
    if (lua_pcall(L, 4, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in update: %s", error_msg);
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "render");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in render: %s", error_msg);
    }
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    if (!L) {
        return;
    }

    lua_getglobal(L, "render_imgui");
    lua_engine::push_engine(L, engine);
    lua_game::push_game(L, game);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        const char *error_msg = lua_tostring(L, -1);
        log_fatal("[LUA] Error in render_imgui: %s", error_msg);
    }
}

} // namespace game

#endif // HAS_LUA
